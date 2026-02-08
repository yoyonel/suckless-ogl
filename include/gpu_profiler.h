#ifndef GPU_PROFILER_H
#define GPU_PROFILER_H

#include "adaptive_sampler.h"
#include "metric_stack.h"
#include "perf_timer.h"
#include <stdint.h>

#define MAX_GPU_STAGES 32
#define MAX_GPU_STAGE_NAME 32
#define GPU_QUERY_BUFFER_COUNT 2

/* --- Capture Settings --- */
#include "app_settings.h"

/**
 * @struct GPUStage
 * @brief Represents a single profiling stage (e.g., "Shadow Map", "G-Buffer").
 */
typedef struct {
	char name[MAX_GPU_STAGE_NAME];
	uint32_t color;
	AdaptiveSampler duration_sampler; /**< Sampler for duration (width). */
	AdaptiveSampler offset_sampler;   /**< Sampler for offset (position). */
	float start_offset_ms;            /**< Target start time (ms). */
	float duration_ms;                /**< Target duration (ms). */
	float prev_start_offset_ms; /**< Previous window start time (ms). */
	float prev_duration_ms;     /**< Previous window duration (ms). */
	float alpha;      /**< Current target alpha (1.0=visible, 0.0=gone). */
	float prev_alpha; /**< Previous window alpha for LERP. */
	int depth;        /**< Hierarchy depth (0=root). */
	int parent_index; /**< Index of parent stage (-1=root). */
} GPUStage;

/**
 * @struct GPUStageInfo
 * @brief Per-frame stage metadata stored alongside queries.
 */
typedef struct {
	char name[MAX_GPU_STAGE_NAME];
	uint32_t color;
	int depth;
	int parent_index;
} GPUStageInfo;

/**
 * @struct GPUQueryBuffer
 * @brief Set of GL queries for one frame, with stage metadata.
 */
typedef struct {
	GPUTimer queries[MAX_GPU_STAGES];
	GPUStageInfo stage_info[MAX_GPU_STAGES];
	int stage_count;
	uint64_t frame_index;
} GPUQueryBuffer;

/**
 * @struct GPUProfiler
 * @brief Manages GPU timing using double-buffered queries to avoid stalls.
 */
typedef struct {
	GPUStage stages[MAX_GPU_STAGES];
	int stage_count; /**< Number of stages from last completed read-back
	                    (used by UI for display). */

	int recording_count; /**< Number of stages recorded this frame
	                        (write-path counter, not for UI). */

	// Double buffering for queries
	GPUQueryBuffer buffers[GPU_QUERY_BUFFER_COUNT];
	int write_index; /**< Index of the buffer we are currently recording to.
	                  */
	int read_index; /**< Index of the buffer we are reading results from. */

	// State tracking for the current frame
	int current_stage_index;
	MetricStack hierarchy_stack; /**< Stack to track hierarchy. */

	float transition_progress; /**< 0.0 to 1.0 animation timer. */
	bool enabled; /**< Whether profiling is currently active. */
} GPUProfiler;

/**
 * @brief Initializes the GPU profiler.
 * @param profiler Pointer to the profiler.
 */
void gpu_profiler_init(GPUProfiler* profiler);

/**
 * @brief Cleans up GPU resources.
 * @param profiler Pointer to the profiler.
 */
void gpu_profiler_cleanup(GPUProfiler* profiler);

/**
 * @brief Marks the beginning of a frame. Swaps buffers and processes previous
 * results.
 * @param profiler Pointer to the profiler.
 * @param frame_index The current frame index.
 */
void gpu_profiler_begin_frame(GPUProfiler* profiler, uint64_t frame_index);

/**
 * @brief Enables or disables the profiler.
 * @param profiler Pointer to the profiler.
 * @param enabled Whether to enable profiling.
 */
void gpu_profiler_set_enabled(GPUProfiler* profiler, bool enabled);

/**
 * @brief Starts a new profiling stage.
 * @param profiler Pointer to the profiler.
 * @param name Name of the stage (must be persistent or copied).
 * @param color Hex color for visualization (e.g., 0xFF0000).
 */
void gpu_profiler_start_stage(GPUProfiler* profiler, const char* name,
                              uint32_t color);

/**
 * @brief Ends the current profiling stage.
 * @param profiler Pointer to the profiler.
 */
void gpu_profiler_end_stage(GPUProfiler* profiler);

/**
 * @brief Resets all samplers for a new capture window.
 */
void gpu_profiler_reset_samplers(GPUProfiler* profiler, double current_time);

/**
 * @struct GPUStageRAII
 * @brief RAII container for automatic GPU stage management.
 */
typedef struct {
	GPUProfiler* profiler;
} GPUStageRAII;

/** @brief Internal cleanup function for GPUStageRAII. */
static inline void gpu_stage_cleanup_raii(GPUStageRAII* stage_raii)
{
	gpu_profiler_end_stage(stage_raii->profiler);
}

/**
 * @brief Scoped GPU profiling stage. Automatically ends on scope exit.
 */
#define GPU_STAGE_PROFILER(profiler_ptr, name, color)                          \
	GPUStageRAII _stage_raii##__LINE__                                     \
	    __attribute__((cleanup(gpu_stage_cleanup_raii))) = {profiler_ptr}; \
	gpu_profiler_start_stage(profiler_ptr, name, color)

#endif  // GPU_PROFILER_H
