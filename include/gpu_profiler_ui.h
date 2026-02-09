#ifndef GPU_PROFILER_UI_H
#define GPU_PROFILER_UI_H

#include "gpu_profiler.h"
#include "ui.h"
#include <stdbool.h>

/**
 * @struct GPUProfilerUI
 * @brief Manages the visual state and animations for the GPU timeline.
 */
typedef struct {
	GPUProfiler display_profiler; /**< Snapshot of profiler data for UI. */
	float box_height;      /**< Current target height of background. */
	float prev_box_height; /**< Previous height for LERP. */
	float global_alpha;    /**< Overall visibility (0.0 to 1.0). */
	int position;          /**< 0 = Top, 1 = Bottom. */
	bool visible;          /**< Target visibility state. */
} GPUProfilerUI;

/**
 * @brief Initializes the GPU timeline UI state.
 */
void gpu_profiler_ui_init(GPUProfilerUI* profiler_ui);

/**
 * @brief Updates animations and synchronizes data with the live profiler.
 */
void gpu_profiler_ui_update(GPUProfilerUI* profiler_ui,
                            GPUProfiler* live_profiler, double delta_time,
                            double current_time, bool should_log_metrics);

/**
 * @brief Renders the GPU timeline using the provided UI context.
 */
void gpu_profiler_ui_draw(GPUProfilerUI* profiler_ui, UIContext* ctx,
                          int screen_width, int screen_height);

/**
 * @brief Toggles the timeline visibility (with animation).
 */
void gpu_profiler_ui_toggle_visibility(GPUProfilerUI* profiler_ui);

/**
 * @brief Toggles the timeline position (Top/Bottom).
 */
void gpu_profiler_ui_toggle_position(GPUProfilerUI* profiler_ui);

#endif /* GPU_PROFILER_UI_H */
