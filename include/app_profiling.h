#ifndef APP_PROFILING_H
#define APP_PROFILING_H

/**
 * @file app_profiling.h
 * @brief Profiling, metrics, and performance monitoring sub-struct.
 *
 * Extracted from app.h to reduce include fan-out.
 * All profiling-related types are grouped here so that app.h
 * only needs a single `#include "app_profiling.h"`.
 */

#include "fps.h"
#include "gpu_profiler.h"
#include "gpu_profiler_ui.h"
#include "gpu_usage.h"
#include "perf_mode.h"
#include "tracy_manager.h"
#include <stdbool.h>

/**
 * @struct AppProfiling
 * @brief Profiling, metrics, and performance monitoring grouped together.
 */
typedef struct AppProfiling {
	FpsCounter fps_counter;       /**< Rolling average FPS manager. */
	GPUProfiler gpu_profiler;     /**< GPU timer query profiler. */
	GPUProfilerUI timeline_ui;    /**< GPU profiler timeline overlay. */
	TracyManager tracy_mgr;       /**< Tracy instrumentation manager. */
	GPUUsageMonitor gpu_usage;    /**< GPU utilization % via DRM fdinfo. */
	PerfModeContext perf_context; /**< Performance mode state context. */
	bool perf_mode_active; /**< Performance/GameMode optimization active. */
	bool log_gpu_metrics;  /**< Toggle console logging of GPU stats. */
} AppProfiling;

/**
 * @brief Initialize all profiling sub-systems to default state.
 * @param prof  Pointer to the profiling sub-struct.
 * @param width  Initial viewport width (for Tracy screenshot buffer).
 * @param height Initial viewport height.
 */
void app_profiling_init(AppProfiling* prof, int width, int height);

/**
 * @brief Release all profiling resources in reverse init order.
 * @param prof  Pointer to the profiling sub-struct.
 */
void app_profiling_cleanup(AppProfiling* prof);

#include "app_subsystem.h"
int app_profiling_subsys_init(struct App* app);
void app_profiling_subsys_cleanup(struct App* app);
#define APP_PROFILING_DESCRIPTOR \
	{"profiling", app_profiling_subsys_init, app_profiling_subsys_cleanup}

#endif /* APP_PROFILING_H */
