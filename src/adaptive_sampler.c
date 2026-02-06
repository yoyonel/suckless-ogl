#include "adaptive_sampler.h"

#include "utils.h"  /* safe_snprintf */
#include <stdint.h> /* uintptr_t */
#include <stdio.h>  /* For FILE* if needed */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* memset */
#include <time.h>   /* time() for seed */

/* --- Minimal PCG32 Implementation --- */
/* http://www.pcg-random.org/ */

static const uint64_t PCG_MULTIPLIER = 6364136223846793005ULL;
static const unsigned int PCG_SHIFT_1 = 18U;
static const unsigned int PCG_SHIFT_2 = 27U;
static const unsigned int PCG_IS_3 = 59U;
static const unsigned int PCG_IS_4 = 31U;
static const size_t DEFAULT_INITIAL_CAPACITY = 64;

static void pcg32_seed(Pcg32* rng, uint64_t initstate, uint64_t initseq)
{
	rng->state = 0U;
	rng->inc = (initseq << 1U) | 1U;
	rng->state = (rng->state * PCG_MULTIPLIER) + rng->inc;
	rng->state += initstate;
	rng->state = (rng->state * PCG_MULTIPLIER) + rng->inc;
}

static uint32_t pcg32_random(Pcg32* rng)
{
	uint64_t oldstate = rng->state;
	rng->state = (oldstate * PCG_MULTIPLIER) + rng->inc;
	uint32_t xorshifted =
	    (uint32_t)(((oldstate >> PCG_SHIFT_1) ^ oldstate) >> PCG_SHIFT_2);
	uint32_t rot = (uint32_t)(oldstate >> PCG_IS_3);
	return ((xorshifted >> rot) |
	        (xorshifted << (((~rot) + 1U) & PCG_IS_4)));
}

/* Returns float in [0, 1) */
static float pcg32_random_float(Pcg32* rng)
{
	/* 1. Generate 32 random bits */
	uint32_t rng_val = pcg32_random(rng);
	/* 2. Convert to float [0,1) using bit manipulation (standard trick
	 * equivalent to divide by 2^32 but faster/canonical) */
	/* Alternative: (float)r / 4294967296.0f; */
	static const float PCG_FLOAT_SCALE = 2.3283064e-10F; /* 1 / 2^32 */
	return (float)rng_val * PCG_FLOAT_SCALE;
}

/* ------------------------------------ */

void adaptive_sampler_init(AdaptiveSampler* sampler, float window_duration,
                           size_t target_samples, float initial_fps_guess)
{
	/* Seed with time or some entropy. For robustness, we combine address +
	 * time */
	uint64_t seed = (uint64_t)time(NULL) ^ (uint64_t)((uintptr_t)sampler);
	static const uint64_t PCG_INIT_SEQ = 54U;
	pcg32_seed(&sampler->rng, seed, PCG_INIT_SEQ);

	sampler->window_duration = window_duration;
	sampler->target_samples = target_samples;
	sampler->samples_taken = 0;
	sampler->window_start_time = 0.0; /* Must be set on reset/first use */
	sampler->window_start_frame = 0;
	sampler->window_end_frame = 0;

	if (initial_fps_guess < 1.0F) {
		static const float DEFAULT_FPS_GUESS = 60.0F;
		initial_fps_guess = DEFAULT_FPS_GUESS;
	}

	sampler->avg_dt = 1.0F / initial_fps_guess;
	static const float DEFAULT_SMOOTHING = 0.15F;
	sampler->alpha = DEFAULT_SMOOTHING; /* Default smoothing factor */

	/* Pre-allocate buffer with some margin (e.g. 2x) to avoid reallocs
	 * usually */
	static const size_t MIN_CAPACITY = 64;
	sampler->capacity = target_samples * 2;
	if (sampler->capacity < MIN_CAPACITY) {
		sampler->capacity = MIN_CAPACITY;
	}

	sampler->samples = (AdaptiveSampleItem*)malloc(
	    sizeof(AdaptiveSampleItem) * sampler->capacity);
	sampler->count = 0;
}

void adaptive_sampler_reset(AdaptiveSampler* sampler, double current_time)
{
	sampler->samples_taken = 0;
	sampler->count = 0; /* Clear vector */
	sampler->window_start_time = current_time;
	// New window - frame index will be set on first sample/should_sample
	sampler->window_start_frame = 0;
	sampler->window_end_frame = 0;
}

int adaptive_sampler_should_sample(AdaptiveSampler* sampler, float delta_time,
                                   double current_time, uint64_t frame_index)
{
	/* If first call (window start 0), reset */
	if (sampler->window_start_time == 0.0) {
		sampler->window_start_time = current_time;
		sampler->window_start_frame = frame_index;
	}

	/* EMA Update */
	sampler->avg_dt = (sampler->alpha * delta_time) +
	                  ((1.0F - sampler->alpha) * sampler->avg_dt);

	double elapsed = current_time - sampler->window_start_time;

	if (elapsed >= (double)sampler->window_duration) {
		return 0; /* Window finished */
	}

	static const float MIN_TIME_LEFT = 0.001F;
	float t_left = (float)((double)sampler->window_duration - elapsed);
	if (t_left < MIN_TIME_LEFT) {
		t_left = MIN_TIME_LEFT;
	}

	/* frames_remaining = T_left / avg_dt */
	static const float MIN_FRAMES_LEFT = 1.0F;
	float expected_frames_left = t_left / sampler->avg_dt;
	if (expected_frames_left < MIN_FRAMES_LEFT) {
		expected_frames_left = MIN_FRAMES_LEFT;
	}

	/* Samples remaining */
	size_t remaining = 0;
	if (sampler->samples_taken < sampler->target_samples) {
		remaining = sampler->target_samples - sampler->samples_taken;
	}

	/* Probability p */
	static const float MAX_PROBABILITY = 1.0F;
	float probability = (float)remaining / expected_frames_left;
	if (probability > MAX_PROBABILITY) {
		probability = MAX_PROBABILITY;
	}

	/* Bernoulli(p) */
	float random_val = pcg32_random_float(&sampler->rng);
	int take = (random_val < probability);

	if (take) {
		/* Add Sample */
		if (sampler->count >= sampler->capacity) {
			/* Grow buffer */
			size_t new_cap = sampler->capacity * 2;
			AdaptiveSampleItem* new_buf =
			    (AdaptiveSampleItem*)realloc(
			        sampler->samples,
			        sizeof(AdaptiveSampleItem) * new_cap);
			if (new_buf) {
				sampler->samples = new_buf;
				sampler->capacity = new_cap;
			} else {
				/* Access error - drop sample */
				return 0;
			}
		}

		AdaptiveSampleItem* item = &sampler->samples[sampler->count];
		item->timestamp = (float)elapsed;
		item->frame_index = frame_index;

		/* Update window end frame */
		sampler->window_end_frame = frame_index;
		if (sampler->window_start_frame == 0) {
			sampler->window_start_frame = frame_index;
		}

		static const float MIN_SAFE_DT = 0.00001F;
		float safe_dt = delta_time;
		if (safe_dt < MIN_SAFE_DT) {
			safe_dt = MIN_SAFE_DT;
		}
		item->value = 1.0F / safe_dt;

		sampler->count++;
		sampler->samples_taken++;
	}

	return take;
}

void adaptive_sampler_add(AdaptiveSampler* sampler, float value,
                          uint64_t frame_index)
{
	if (!sampler) {
		return;
	}

	if (sampler->count >= sampler->capacity) {
		/* Grow buffer */
		size_t new_cap = sampler->capacity * 2;
		if (new_cap == 0) {
			new_cap = DEFAULT_INITIAL_CAPACITY;  // Safety
		}

		AdaptiveSampleItem* new_buf = (AdaptiveSampleItem*)realloc(
		    sampler->samples, sizeof(AdaptiveSampleItem) * new_cap);
		if (new_buf) {
			sampler->samples = new_buf;
			sampler->capacity = new_cap;
		} else {
			return; /* Allocation failed */
		}
	}

	AdaptiveSampleItem* item = &sampler->samples[sampler->count];
	item->timestamp =
	    0.0F;  // Placeholder as we don't have relative time here
	item->value = value;
	item->frame_index = frame_index;

	/* Update window end frame */
	sampler->window_end_frame = frame_index;
	if (sampler->window_start_frame == 0) {
		sampler->window_start_frame = frame_index;
	}
	sampler->count++;
	sampler->samples_taken++;
}

int adaptive_sampler_is_finished(const AdaptiveSampler* sampler,
                                 double current_time)
{
	if (sampler->window_start_time == 0.0) {
		return 0;
	}
	double elapsed = current_time - sampler->window_start_time;
	return elapsed >= (double)sampler->window_duration;
}

float adaptive_sampler_get_average(const AdaptiveSampler* sampler)
{
	if (sampler->count == 0) {
		return 0.0F;
	}
	float sum = 0.0F;
	for (size_t i = 0; i < sampler->count; i++) {
		sum += sampler->samples[i].value;
	}
	return sum / (float)sampler->count;
}

size_t adaptive_sampler_get_sample_count(const AdaptiveSampler* sampler)
{
	return sampler->count;
}

void adaptive_sampler_get_window_range(const AdaptiveSampler* sampler,
                                       uint64_t* start_frame,
                                       uint64_t* end_frame)
{
	if (!sampler) {
		return;
	}
	if (start_frame) {
		*start_frame = sampler->window_start_frame;
	}
	if (end_frame) {
		*end_frame = sampler->window_end_frame;
	}
}

size_t adaptive_sampler_get_sample_indices(const AdaptiveSampler* sampler,
                                           uint64_t* out_indices,
                                           size_t max_count)
{
	if (!sampler || !out_indices || max_count == 0) {
		return 0;
	}

	size_t count = sampler->count;
	if (count > max_count) {
		count = max_count;
	}

	for (size_t i = 0; i < count; ++i) {
		out_indices[i] = sampler->samples[i].frame_index;
	}

	return count;
}

void adaptive_sampler_cleanup(AdaptiveSampler* sampler)
{
	if (sampler->samples) {
		free(sampler->samples);
		sampler->samples = NULL;
	}
	sampler->count = 0;
	sampler->capacity = 0;
}
