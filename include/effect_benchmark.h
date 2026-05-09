/**
 * @file effect_benchmark.h
 * @brief Automated A/B GPU cost measurement for individual postprocess effects.
 *
 * Cycles through each toggleable postprocess effect, disabling it one at a time
 * while measuring the "Final Composite" GPU timer. The difference between the
 * baseline (all effects ON) and each disabled-effect run gives the per-effect
 * GPU cost.
 *
 * Usage:
 *   1. Call effect_benchmark_start() to begin a sweep (e.g. on key press).
 *   2. Call effect_benchmark_update() every frame after
 * gpu_profiler_begin_frame.
 *   3. Results are logged to console and displayed via notification.
 */

#ifndef EFFECT_BENCHMARK_H
#define EFFECT_BENCHMARK_H

#include <stdbool.h>

typedef struct PostProcess PostProcess;
typedef struct GPUProfiler GPUProfiler;

/** @brief Number of frames to measure per phase (warmup excluded). */
#define BENCH_MEASURE_FRAMES 120

/** @brief Frames to skip at the start of each phase (cache warmup). */
#define BENCH_WARMUP_FRAMES 30

/** @brief Maximum number of effects that can be benchmarked. */
#define BENCH_MAX_EFFECTS 16

/** @brief Timeout in milliseconds to abort benchmark if no profiler data. */
#define BENCH_TIMEOUT_MS 2000.0F

/**
 * @enum BenchPhase
 * @brief State machine phases for the benchmark sweep.
 */
typedef enum {
	BENCH_IDLE = 0,  /**< No benchmark running. */
	BENCH_BASELINE,  /**< Measuring with all effects at original state. */
	BENCH_STABILIZE, /**< Restoring baseline between tests (warmup only). */
	BENCH_EFFECT_TEST, /**< Measuring with one effect disabled. */
	BENCH_DONE         /**< All phases complete, results ready. */
} BenchPhase;

/**
 * @struct EffectBenchResult
 * @brief Timing result for a single effect.
 */
typedef struct EffectBenchResult {
	const char* name;        /**< Human-readable effect name. */
	unsigned int effect_bit; /**< PostProcessEffect bitmask value. */
	float mean_ms;           /**< Mean composite time with effect OFF. */
	float stddev_ms;         /**< Standard deviation. */
	float cost_ms;           /**< Estimated cost = baseline - this mean. */
} EffectBenchResult;

/**
 * @struct EffectBenchmark
 * @brief State for the automated benchmark sweep.
 */
typedef struct EffectBenchmark {
	BenchPhase phase;
	PostProcess* postprocess;
	GPUProfiler* profiler;

	/* Original state (restored at end) */
	unsigned int saved_effects;

	/* Benchmark mask: all benchmarkable effects forced ON */
	unsigned int benchmark_effects;

	/* Effect table */
	struct {
		const char* name;
		unsigned int bit;
	} effects[BENCH_MAX_EFFECTS];

	int effect_count;

	/* Current sweep position */
	int current_effect_idx; /**< -1 = baseline, 0..N = effect test. */
	int frame_counter;      /**< Frames elapsed in current phase. */
	float timeout_timer; /**< Ms elapsed since last valid profile data. */

	/* Accumulation */
	double sum_ms;
	double sum_sq_ms;
	int sample_count;

	/* Results */
	float baseline_mean_ms;
	float baseline_stddev_ms;
	EffectBenchResult results[BENCH_MAX_EFFECTS];
	int result_count;
} EffectBenchmark;

/**
 * @brief Initializes the benchmark (call once at startup).
 */
void effect_benchmark_init(EffectBenchmark* bench, PostProcess* postprocess,
                           GPUProfiler* profiler);

/**
 * @brief Starts a new benchmark sweep.
 * @return true if started, false if already running.
 */
bool effect_benchmark_start(EffectBenchmark* bench);

/**
 * @brief Per-frame update. Call after gpu_profiler_begin_frame().
 *
 * Reads the "Final Composite" timing from the profiler, advances the
 * state machine, and toggles effects as needed.
 *
 * @return true if the sweep just completed this frame, false otherwise.
 */
bool effect_benchmark_update(EffectBenchmark* bench);

/**
 * @brief Returns true if a benchmark is currently running.
 */
bool effect_benchmark_is_running(const EffectBenchmark* bench);

/**
 * @brief Logs all results to the console via LOG_INFO.
 */
void effect_benchmark_log_results(const EffectBenchmark* bench);

#endif /* EFFECT_BENCHMARK_H */
