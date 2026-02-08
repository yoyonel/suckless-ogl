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
#define MS_PER_SEC 1000.0F

/* ---- Effect table (only toggleable fragment-shader effects) ---- */

typedef struct {
	const char* name;
	unsigned int bit;
} EffectEntry;

/**
 * All toggleable effects executed in the "Final Composite" fullscreen pass
 * (postprocess.frag). This includes multi-pass effects like Bloom, DoF, and
 * Motion Blur whose setup runs in separate profiler stages but whose final
 * compositing/sampling cost is paid here.
 *
 * Order follows the pipeline stages in postprocess.frag main().
 */
/* Early-exit debug views that short-circuit the entire pipeline.
 * These are NOT benchmarkable — enabling them replaces all output
 * with a debug visualisation and makes cost measurements meaningless.
 * Excluded bits: POSTFX_MOTION_BLUR_DEBUG, POSTFX_VECTOR_FIELD_DEBUG */
static const unsigned int DEBUG_VIEW_BITS =
    (unsigned int)POSTFX_MOTION_BLUR_DEBUG |
    (unsigned int)POSTFX_VECTOR_FIELD_DEBUG | (unsigned int)POSTFX_DOF_DEBUG |
    (unsigned int)POSTFX_EXPOSURE_DEBUG | (unsigned int)POSTFX_FXAA_DEBUG;

static const EffectEntry BENCHMARKABLE_EFFECTS[] = {
    /* Scene source: Motion Blur → Chromatic Aberration → FXAA */
    {"Motion Blur", POSTFX_MOTION_BLUR},
    {"Chromatic Aberration", POSTFX_CHROM_ABBR},
    {"FXAA", POSTFX_FXAA},
    /* Depth of Field */
    {"DOF", POSTFX_DOF},
    /* HDR pipeline */
    {"Bloom", POSTFX_BLOOM},
    {"Exposure", POSTFX_EXPOSURE},
    {"Auto Exposure", POSTFX_AUTO_EXPOSURE},
    {"Color Grading", POSTFX_COLOR_GRADING},
    /* LDR pipeline (post-tonemap) */
    {"Vignette", POSTFX_VIGNETTE},
    {"Banding", POSTFX_BANDING},
    {"Grain", POSTFX_GRAIN},
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

/**
 * @brief Atomically apply a new effect bitmask.
 *
 * Sets active_effects, marks the UBO dirty, AND recompiles the
 * uber-shader if running in optimized mode (release builds bake
 * effect toggles as compile-time #defines — without recompilation
 * the shader ignores the UBO activeEffects field).
 */
static void apply_effects(EffectBenchmark* bench, unsigned int effects)
{
	bench->postprocess->active_effects = effects;
	bench->postprocess->ubo_dirty = true;
	if (bench->postprocess->is_optimized) {
		postprocess_compile_optimized(bench->postprocess, effects);
	}
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
	         "%d+%d frames/phase, all effects forced ON) ===",
	         bench->effect_count, BENCH_WARMUP_FRAMES,
	         BENCH_MEASURE_FRAMES);

	bench->saved_effects = bench->postprocess->active_effects;

	/* Force ALL benchmarkable effects ON, but keep debug views OFF
	 * (they do early-return and would shadow the whole pipeline). */
	unsigned int all_bits = bench->saved_effects;
	for (int i = 0; i < bench->effect_count; ++i) {
		all_bits |= bench->effects[i].bit;
	}
	all_bits &= ~DEBUG_VIEW_BITS;
	bench->benchmark_effects = all_bits;
	apply_effects(bench, all_bits);

	bench->result_count = 0;
	bench->current_effect_idx = -1; /* -1 = baseline */
	bench->phase = BENCH_BASELINE;
	bench->frame_counter = 0;
	bench->timeout_timer = 0.0F;

	/* Accumulation */
	reset_accumulator(bench);

	return true;
}

bool effect_benchmark_is_running(const EffectBenchmark* bench)
{
	return bench->phase == BENCH_BASELINE ||
	       bench->phase == BENCH_STABILIZE ||
	       bench->phase == BENCH_EFFECT_TEST;
}

bool effect_benchmark_update(EffectBenchmark* bench)
{
	if (bench->phase == BENCH_IDLE || bench->phase == BENCH_DONE) {
		return false;
	}

	bench->frame_counter++;

	/* ---- STABILIZE phase: baseline restored, just wait for warmup ---- */
	if (bench->phase == BENCH_STABILIZE) {
		if (bench->frame_counter < BENCH_WARMUP_FRAMES) {
			return false;
		}
		/* Warmup done — disable next effect and start measuring */
		apply_effects(
		    bench, bench->benchmark_effects &
		               ~bench->effects[bench->current_effect_idx].bit);
		reset_accumulator(bench);
		bench->phase = BENCH_EFFECT_TEST;

		LOG_INFO(BENCH_TAG, "Testing: %s OFF...",
		         bench->effects[bench->current_effect_idx].name);
		return false;
	}

	/* Skip warmup frames for BASELINE and EFFECT_TEST */
	if (bench->frame_counter <= BENCH_WARMUP_FRAMES) {
		return false;
	}

	/* Read the \"Final Composite\" timing from the previous frame */
	/* Read the "Final Composite" timing from the previous frame */
	float composite_ms = find_composite_duration(bench->profiler);
	if (composite_ms < 0.0F) {
		/* Profiler hasn't produced results yet */
		bench->timeout_timer +=
		    bench->postprocess->delta_time * MS_PER_SEC;

		if (bench->timeout_timer > BENCH_TIMEOUT_MS) {
			LOG_ERROR(
			    BENCH_TAG,
			    "Benchmark Timed Out! No data from GPU Profiler "
			    "for %.1f ms.",
			    bench->timeout_timer);
			/* Abort: restore original state */
			apply_effects(bench, bench->saved_effects);
			bench->phase = BENCH_DONE;
			return true;
		}
		return false;
	}

	/* Data received — reset timeout */
	bench->timeout_timer = 0.0F;

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

		if (bench->current_effect_idx >= bench->effect_count) {
			apply_effects(bench, bench->saved_effects);
			bench->phase = BENCH_DONE;
			effect_benchmark_log_results(bench);
			return true;
		}

		/* Go through stabilize before first test to ensure
		 * identical conditions as subsequent tests. */
		apply_effects(bench, bench->benchmark_effects);
		bench->frame_counter = 0;
		bench->phase = BENCH_STABILIZE;
		return false;
	}

	/* BENCH_EFFECT_TEST: store result for current effect */
	{
		EffectBenchResult* res = &bench->results[bench->result_count++];
		res->name = bench->effects[bench->current_effect_idx].name;
		res->effect_bit = bench->effects[bench->current_effect_idx].bit;
		compute_stats(bench, &res->mean_ms, &res->stddev_ms);
		res->cost_ms = bench->baseline_mean_ms - res->mean_ms;

		LOG_INFO(BENCH_TAG, "  %s OFF: %.4f ms (±%.4f) → cost: %.4f ms",
		         res->name, res->mean_ms, res->stddev_ms, res->cost_ms);
	}

	/* Advance to next effect */
	bench->current_effect_idx++;

	if (bench->current_effect_idx >= bench->effect_count) {
		/* All done — restore original state */
		apply_effects(bench, bench->saved_effects);
		bench->phase = BENCH_DONE;
		effect_benchmark_log_results(bench);
		return true;
	}

	/* Restore baseline (all ON) and stabilize before next test */
	apply_effects(bench, bench->benchmark_effects);
	bench->frame_counter = 0;
	bench->phase = BENCH_STABILIZE;
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
		LOG_INFO(BENCH_TAG, "║ %-18s ║ %+8.4f ║   ±%.4f ║   ON   ║",
		         res->name, res->cost_ms, res->stddev_ms);
		total_cost += res->cost_ms;
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
