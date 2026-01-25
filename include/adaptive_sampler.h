#ifndef ADAPTIVE_SAMPLER_H
#define ADAPTIVE_SAMPLER_H

#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t

/* Simple Sample struct */
typedef struct {
	float timestamp; /* Time elapsed since window start */
	float value;     /* Measured value (e.g. FPS) */
} AdaptiveSampleItem;

/* PCG32 State for fast RNG */
typedef struct {
	uint64_t state;
	uint64_t inc;
} Pcg32;

typedef struct {
	/* RNG State */
	Pcg32 rng;

	/* Configuration */
	float window_duration; /* in seconds */
	size_t target_samples; /* Expected sample count */

	/* State */
	size_t samples_taken;
	double window_start_time; /* absolute time of window start */
	float avg_dt;             /* Exponential Moving Average of dt */
	float alpha;              /* EMA smoothing factor */

	/* Buffer (Dynamic) */
	AdaptiveSampleItem* samples;
	size_t capacity;
	size_t count;

} AdaptiveSampler;

/* API */
void adaptive_sampler_init(AdaptiveSampler* sampler, float window_duration,
                           size_t target_samples, float initial_fps_guess);

/* Returns 1 if sampled, 0 otherwise. 'current_time' is absolute time in
 * seconds. */
int adaptive_sampler_should_sample(AdaptiveSampler* sampler, float delta_time,
                                   double current_time);

/* Metrics Query API */
int adaptive_sampler_is_finished(const AdaptiveSampler* sampler,
                                 double current_time);
float adaptive_sampler_get_average(const AdaptiveSampler* sampler);
size_t adaptive_sampler_get_sample_count(const AdaptiveSampler* sampler);

void adaptive_sampler_reset(AdaptiveSampler* sampler, double current_time);

/* Helper to convert samples to ASCII string (caller must free result) */
/* width: character width of the timeline */
/* avg_value: value to compare against for +/- signs */
void adaptive_sampler_ascii_plot(const AdaptiveSampler* sampler, char* buffer,
                                 size_t buffer_size, size_t width,
                                 float avg_value);

void adaptive_sampler_cleanup(AdaptiveSampler* sampler);

#endif
