/**
 * @file perf_timer.h
 * @brief High-precision performance measurement (CPU and GPU).
 *
 * This module provides timers for measuring CPU wall-clock time using
 * clock_gettime and GPU execution time using OpenGL timer queries.
 * It also includes RAII-style macros for automatic profiling.
 */

#ifndef PERF_TIMER_H
#define PERF_TIMER_H

#include <glad/glad.h>

#include <time.h>

/**
 * @struct PerfTimer
 * @brief CPU high-precision timer.
 *
 * Uses clock_gettime(CLOCK_MONOTONIC) for accurate CPU timing.
 * Typical resolution: nanoseconds.
 */
typedef struct {
	struct timespec start; /**< Recording start time. */
	struct timespec end;   /**< Recording end time. */
} PerfTimer;

/**
 * @brief GPU performance timer using OpenGL Query Objects.
 *
 * Measures actual GPU execution time, which can differ significantly
 * from CPU "wall-clock" time due to driver buffering and parallelism.
 */
typedef struct {
	GLuint query_start; /**< Handle for the start timestamp query. */
	GLuint query_end;   /**< Handle for the end timestamp query. */
	int active; /**< Flag indicating if a measurement is in progress. */
} GPUTimer;

#include <tracy/TracyC.h>

/**
 * @struct HybridTimer
 * @brief Hybrid timer combining CPU and GPU measurements.
 */
typedef struct {
	PerfTimer cpu;           /**< CPU timer. */
	GPUTimer gpu;            /**< GPU timer. */
	TracyCZoneCtx tracy_ctx; /**< Tracy profiling context (Outer). */
	TracyCZoneCtx host_ctx;  /**< Tracy profiling context (Inner Host). */
} HybridTimer;

/* ========================================================================= */
/* CPU Timer API                                                             */
/* ========================================================================= */

/**
 * @brief Starts the CPU timer.
 * @param timer Pointer to the timer.
 */
void perf_timer_start(PerfTimer* timer);

/**
 * @brief Stops the timer and returns elapsed time in milliseconds.
 * @param timer Pointer to the timer.
 * @return Elapsed time (double precision).
 */
double perf_timer_elapsed_ms(PerfTimer* timer);

/**
 * @brief Stops the timer and returns elapsed time in microseconds.
 * @param timer Pointer to the timer.
 * @return Elapsed time (double precision).
 */
double perf_timer_elapsed_us(PerfTimer* timer);

/**
 * @brief Stops the timer and returns elapsed time in seconds.
 * @param timer Pointer to the timer.
 * @return Elapsed time (double precision).
 */
double perf_timer_elapsed_s(PerfTimer* timer);

/* ========================================================================= */
/* GPU Timer API                                                             */
/* ========================================================================= */

/**
 * @brief Initializes and starts a GPU measurement.
 * @param timer Pointer to the timer.
 */
void gpu_timer_start(GPUTimer* timer);

/**
 * @brief Explicitly stops the GPU timer (records end timestamp).
 * @param timer Pointer to the timer.
 */
void gpu_timer_stop(GPUTimer* timer);

/**
 * @brief Stops the GPU timer and retrieves the result.
 * @param timer Pointer to the timer.
 * @param wait_for_result If true, blocks until the GPU is finished and result
 * is ready.
 * @return Elapsed time in milliseconds, or -1.0 if not ready and
 * wait_for_result is false.
 */
double gpu_timer_elapsed_ms(GPUTimer* timer, int wait_for_result);

/**
 * @brief Releases OpenGL resources associated with the GPU timer.
 * @param timer Pointer to the timer.
 */
void gpu_timer_cleanup(GPUTimer* timer);

/* ========================================================================= */
/* Hybrid Timer API                                                          */
/* ========================================================================= */

/**
 * @brief Starts both CPU and GPU measurement simultaneously.
 * @return Initialized and started HybridTimer structure.
 */
HybridTimer perf_hybrid_start(void);

/**
 * @brief Stops both measurements and logs the results to the console.
 * @param timer Pointer to the timer.
 * @param label Descriptive string for the log entry.
 */
void perf_hybrid_stop(HybridTimer* timer, const char* label);

/* ========================================================================= */
/* Macro Helpers                                                             */
/* ========================================================================= */

/**
 * @brief Automatically measures a code block and stores result in `var_name`.
 *
 * Usage:
 * @code
 *   PERF_MEASURE_MS(load_time) {
 *       // Code to measure
 *   }
 *   printf("Took %.2f ms\n", load_time);
 * @endcode
 */
#define PERF_MEASURE_MS(var_name)                                              \
	double var_name = 0.0;                                                 \
	for (PerfTimer                                                         \
	         _timer##var_name = {0},                                       \
	         *_run = (perf_timer_start(&_timer##var_name), (PerfTimer*)1); \
	     _run;                                                             \
	     var_name = perf_timer_elapsed_ms(&_timer##var_name), _run = NULL)

/**
 * @brief Automatically measures and logs a code block to the "perf" category.
 */
#define PERF_MEASURE_LOG(label)                                            \
	for (PerfTimer _timer = {0},                                       \
	               *_run = (perf_timer_start(&_timer), (PerfTimer*)1); \
	     _run; LOG_INFO("perf", "%s: %.2f ms", label,                  \
	                    perf_timer_elapsed_ms(&_timer)),               \
	               _run = NULL)

/**
 * @brief Automatically measures a GPU block and stores result in `var_name`.
 */
#define GPU_MEASURE_MS(var_name)                                           \
	double var_name = 0.0;                                             \
	for (GPUTimer _gpu_timer##var_name = {0},                          \
	              *_gpu_run = (gpu_timer_start(&_gpu_timer##var_name), \
	                           (GPUTimer*)1);                          \
	     _gpu_run;                                                     \
	     var_name = gpu_timer_elapsed_ms(&_gpu_timer##var_name, 1),    \
	              gpu_timer_cleanup(&_gpu_timer##var_name),            \
	              _gpu_run = NULL)

/**
 * @brief Automatically measures and logs a GPU block to "perf.gpu".
 */
#define GPU_MEASURE_LOG(label)                                             \
	for (GPUTimer                                                      \
	         _gpu_timer = {0},                                         \
	         *_gpu_run = (gpu_timer_start(&_gpu_timer), (GPUTimer*)1); \
	     _gpu_run; LOG_INFO("perf.gpu", "%s: %.2f ms", label,          \
	                        gpu_timer_elapsed_ms(&_gpu_timer, 1)),     \
	         gpu_timer_cleanup(&_gpu_timer), _gpu_run = NULL)

/**
 * @brief Hybrid measurement macro. Useful for identifying driver vs GPU
 * bottlenecks.
 */
#define HYBRID_MEASURE_LOG(label)                                             \
	for (HybridTimer _h = perf_hybrid_start(), *_h_run = (HybridTimer*)1; \
	     _h_run; perf_hybrid_stop(&_h, label), _h_run = NULL)

/**
 * @struct HybridTimerRAII
 * @brief Internal helper for scope-based timing.
 */
typedef struct {
	HybridTimer timer;
	const char* label;
} HybridTimerRAII;

/** @brief Internal cleanup function for HybridTimerRAII. */
static inline void hybrid_timer_cleanup_raii(HybridTimerRAII* timer_raii)
{
	perf_hybrid_stop(&timer_raii->timer, timer_raii->label);
}

/**
 * @brief Scoped hybrid timer. Automatically logs on scope exit.
 *
 * Usage:
 * @code
 *   void complex_function() {
 *       HYBRID_FUNC_TIMER("IBL: Specular Map");
 *       // ... function body ...
 *   }
 * @endcode
 */
#define HYBRID_FUNC_TIMER(label)                                    \
	HybridTimerRAII _h_raii                                     \
	    __attribute__((cleanup(hybrid_timer_cleanup_raii))) = { \
	        perf_hybrid_start(), label}

#endif /* PERF_TIMER_H */
