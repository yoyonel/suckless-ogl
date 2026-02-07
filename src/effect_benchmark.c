/**
 * @file effect_benchmark.c
 * @brief Automated A/B GPU cost measurement for postprocess effects.
 *
 * State machine that cycles through effects, measuring "Final Composite"
 * GPU time with each effect disabled vs baseline (all ON).
 */

#include "effect_benchmark.h"

#include "log.h"
#include <math.h>
#include <string.h>

#define BENCH_TAG "suckless-ogl.bench"

/* ---- Effect table (only toggleable fragment-shader effects) ---- */

typedef struct {
	const char* name;
	unsigned int bit;
} EffectEntry;

/**
 * Effects that run inside the "Final Composite" draw call.
 * Multi-pass effects (Bloom, DoF, AutoExposure, MotionBlur) already have
 * their own profiler stages — no need to A/B them.
 */
static const EffectEntry BENCHMARKABLE_EFFECTS[] = {
    {"FXAA", POSTFX_FXAA},
    {"Chromatic Aberration", POSTFX_CHROM_ABBR},
    {"Vignette", POSTFX_VIGNETTE},
    {"Grain", POSTFX_GRAIN},
    {"Color Grading", POSTFX_COLOR_GRADING},
    {"Banding", POSTFX_BANDING},
    {"Exposure", POSTFX_EXPOSURE},
};

static const int BENCHMARKABLE_COUNT =
    (int)(sizeof(BENCHMARKABLE_EFFECTS) / sizeof(BENCHMARKABLE_EFFECTS[0]));

/* ---- Helpers ---- */

/**
 * @brief Find the "Final Composite" stage in the profiler and return its
 *        raw duration_ms, or -1.0 if not found.
 */
static float find_composite_duration(const GPUProfiler* profiler)
{
	for (int i = 0; i < profiler->stage_count; ++i) {
		if (strcmp(profiler->stages[i].name, "Final Composite") == 0) {
			return profiler->stages[i].duration_ms;
		}
	}
	return -1.0F;
}

static void reset_accumulator(EffectBenchmark* bench)
{
	bench->sum_ms = 0.0;
	bench->sum_sq_ms = 0.0;
	bench->sample_count = 0;
	bench->frame_counter = 0;
}

static void compute_stats(const EffectBenchmark* bench, float* out_mean,
                          float* out_stddev)
{
	if (bench->sample_count < 2) {
		*out_mean =
		    (bench->sample_count == 1) ? (float)bench->sum_ms : 0.0F;
		*out_stddev = 0.0F;
		return;
	}
	double mean = bench->sum_ms / (double)bench->sample_count;
	double variance = (bench->sum_sq_ms - (bench->sum_ms * bench->sum_ms /
	                                       (double)bench->sample_count)) /
	                  (double)(bench->sample_count - 1);
	if (variance < 0.0) {
		variance = 0.0;
	}
	*out_mean = (float)mean;
	*out_stddev = (float)sqrt(variance);
}

/* ---- Public API ---- */

void effect_benchmark_init(EffectBenchmark* bench, PostProcess* postprocess,
                           GPUProfiler* profiler)
{
	*bench = (EffectBenchmark){0};
	bench->postprocess = postprocess;
	bench->profiler = profiler;
	bench->phase = BENCH_IDLE;

	/* Build the effect table from the static list */
	bench->effect_count = 0;
	for (int i = 0; i < BENCHMARKABLE_COUNT && i < BENCH_MAX_EFFECTS; ++i) {
		bench->effects[bench->effect_count].name =
		    BENCHMARKABLE_EFFECTS[i].name;
		bench->effects[bench->effect_count].bit =
		    BENCHMARKABLE_EFFECTS[i].bit;
		bench->effect_count++;
	}
}

bool effect_benchmark_start(EffectBenchmark* bench)
{
	if (bench->phase != BENCH_IDLE && bench->phase != BENCH_DONE) {
		return false; /* Already running */
	}

	LOG_INFO(BENCH_TAG,
	         "=== Effect Benchmark: Starting sweep (%d effects, "
	         "%d+%d frames/phase) ===",
	         bench->effect_count, BENCH_WARMUP_FRAMES,
	         BENCH_MEASURE_FRAMES);

	bench->saved_effects = bench->postprocess->active_effects;
	bench->result_count = 0;
	bench->current_effect_idx = -1; /* -1 = baseline */
	bench->phase = BENCH_BASELINE;
	reset_accumulator(bench);

	return true;
}

bool effect_benchmark_is_running(const EffectBenchmark* bench)
{
	return bench->phase == BENCH_BASELINE ||
	       bench->phase == BENCH_EFFECT_TEST;
}

bool effect_benchmark_update(EffectBenchmark* bench)
{
	if (bench->phase == BENCH_IDLE || bench->phase == BENCH_DONE) {
		return false;
	}

	bench->frame_counter++;

	/* Skip warmup frames */
	if (bench->frame_counter <= BENCH_WARMUP_FRAMES) {
		return false;
	}

	/* Read the "Final Composite" timing from the previous frame */
	float composite_ms = find_composite_duration(bench->profiler);
	if (composite_ms < 0.0F) {
		/* Profiler hasn't produced results yet, skip */
		return false;
	}

	/* Accumulate */
	bench->sum_ms += (double)composite_ms;
	bench->sum_sq_ms += (double)composite_ms * (double)composite_ms;
	bench->sample_count++;

	/* Check if this phase is complete */
	if (bench->sample_count < BENCH_MEASURE_FRAMES) {
		return false;
	}

	/* ---- Phase complete: compute stats and advance ---- */

	if (bench->phase == BENCH_BASELINE) {
		compute_stats(bench, &bench->baseline_mean_ms,
		              &bench->baseline_stddev_ms);
		LOG_INFO(BENCH_TAG, "Baseline: %.4f ms (±%.4f ms) [%d samples]",
		         bench->baseline_mean_ms, bench->baseline_stddev_ms,
		         bench->sample_count);

		/* Move to first effect */
		bench->current_effect_idx = 0;
		bench->phase = BENCH_EFFECT_TEST;

		/* Skip effects that are not currently active */
		while (bench->current_effect_idx < bench->effect_count) {
			unsigned int bit =
			    bench->effects[bench->current_effect_idx].bit;
			if (bench->saved_effects & bit) {
				break; /* This effect is active, test it */
			}
			/* Record as "not tested" */
			EffectBenchResult* res =
			    &bench->results[bench->result_count++];
			res->name =
			    bench->effects[bench->current_effect_idx].name;
			res->effect_bit = bit;
			res->was_active = false;
			res->mean_ms = 0.0F;
			res->stddev_ms = 0.0F;
			res->cost_ms = 0.0F;
			bench->current_effect_idx++;
		}

		if (bench->current_effect_idx >= bench->effect_count) {
			/* No active effects to test */
			bench->postprocess->active_effects =
			    bench->saved_effects;
			bench->postprocess->ubo_dirty = true;
			bench->phase = BENCH_DONE;
			effect_benchmark_log_results(bench);
			return true;
		}

		/* Disable the first testable effect */
		bench->postprocess->active_effects =
		    bench->saved_effects &
		    ~bench->effects[bench->current_effect_idx].bit;
		bench->postprocess->ubo_dirty = true;
		reset_accumulator(bench);

		LOG_INFO(BENCH_TAG, "Testing: %s OFF...",
		         bench->effects[bench->current_effect_idx].name);
		return false;
	}

	/* BENCH_EFFECT_TEST: store result for current effect */
	{
		EffectBenchResult* res = &bench->results[bench->result_count++];
		res->name = bench->effects[bench->current_effect_idx].name;
		res->effect_bit = bench->effects[bench->current_effect_idx].bit;
		res->was_active = true;
		compute_stats(bench, &res->mean_ms, &res->stddev_ms);
		res->cost_ms = bench->baseline_mean_ms - res->mean_ms;

		LOG_INFO(BENCH_TAG, "  %s OFF: %.4f ms (±%.4f) → cost: %.4f ms",
		         res->name, res->mean_ms, res->stddev_ms, res->cost_ms);
	}

	/* Advance to next active effect */
	bench->current_effect_idx++;
	while (bench->current_effect_idx < bench->effect_count) {
		unsigned int bit =
		    bench->effects[bench->current_effect_idx].bit;
		if (bench->saved_effects & bit) {
			break; /* Active, test it */
		}
		/* Skip inactive effects */
		EffectBenchResult* res = &bench->results[bench->result_count++];
		res->name = bench->effects[bench->current_effect_idx].name;
		res->effect_bit = bit;
		res->was_active = false;
		res->mean_ms = 0.0F;
		res->stddev_ms = 0.0F;
		res->cost_ms = 0.0F;
		bench->current_effect_idx++;
	}

	if (bench->current_effect_idx >= bench->effect_count) {
		/* All done — restore original state */
		bench->postprocess->active_effects = bench->saved_effects;
		bench->postprocess->ubo_dirty = true;
		bench->phase = BENCH_DONE;
		effect_benchmark_log_results(bench);
		return true;
	}

	/* Set up next effect test */
	bench->postprocess->active_effects =
	    bench->saved_effects &
	    ~bench->effects[bench->current_effect_idx].bit;
	bench->postprocess->ubo_dirty = true;
	reset_accumulator(bench);

	LOG_INFO(BENCH_TAG, "Testing: %s OFF...",
	         bench->effects[bench->current_effect_idx].name);
	return false;
}

void effect_benchmark_log_results(const EffectBenchmark* bench)
{
	LOG_INFO(BENCH_TAG, "");
	LOG_INFO(BENCH_TAG,
	         "╔══════════════════════════════════════════════════════╗");
	LOG_INFO(BENCH_TAG,
	         "║       POSTPROCESS EFFECT BENCHMARK RESULTS         ║");
	LOG_INFO(BENCH_TAG,
	         "╠══════════════════════════════════════════════════════╣");
	LOG_INFO(BENCH_TAG, "║ Baseline (all ON): %8.4f ms (±%.4f ms)     ║",
	         bench->baseline_mean_ms, bench->baseline_stddev_ms);
	LOG_INFO(BENCH_TAG,
	         "╠════════════════════╦═══════════╦═══════════╦════════╣");
	LOG_INFO(BENCH_TAG,
	         "║ Effect             ║  Cost(ms) ║ StdDev    ║ Status ║");
	LOG_INFO(BENCH_TAG,
	         "╠════════════════════╬═══════════╬═══════════╬════════╣");

	float total_cost = 0.0F;

	for (int i = 0; i < bench->result_count; ++i) {
		const EffectBenchResult* res = &bench->results[i];
		if (res->was_active) {
			LOG_INFO(BENCH_TAG,
			         "║ %-18s ║ %+8.4f ║   ±%.4f ║   ON   ║",
			         res->name, res->cost_ms, res->stddev_ms);
			total_cost += res->cost_ms;
		} else {
			LOG_INFO(BENCH_TAG,
			         "║ %-18s ║     —    ║     —    ║  OFF   ║",
			         res->name);
		}
	}

	LOG_INFO(BENCH_TAG,
	         "╠════════════════════╬═══════════╬═══════════╬════════╣");
	LOG_INFO(BENCH_TAG,
	         "║ Sum of costs       ║ %+8.4f ║           ║        ║",
	         total_cost);
	LOG_INFO(BENCH_TAG,
	         "╚════════════════════╩═══════════╩═══════════╩════════╝");
	LOG_INFO(BENCH_TAG, "");
	LOG_INFO(BENCH_TAG,
	         "Note: Positive cost = effect adds GPU time. "
	         "Sum may not equal baseline due to shared ALU/cache.");
}
