#ifndef GPU_PROFILER_H
#define GPU_PROFILER_H

#include "adaptive_sampler.h"
#include "perf_timer.h"
#include <stdint.h>

#define MAX_GPU_STAGES 32
#define GPU_QUERY_BUFFER_COUNT 2

/**
 * @struct GPUStage
 * @brief Represents a single profiling stage (e.g., "Shadow Map", "G-Buffer").
 */
typedef struct {
	char name[32];
	uint32_t color;
	AdaptiveSampler sampler; /**< Sliding window sampler for smoothing. */
	double start_offset_ms; /**< Start time relative to frame start (ms). */
	double duration_ms;     /**< Last measured duration (ms). */
} GPUStage;

/**
 * @struct GPUQueryBuffer
 * @brief Set of GL queries for one frame.
 */
typedef struct {
	GPUTimer queries[MAX_GPU_STAGES];
} GPUQueryBuffer;

/**
 * @struct GPUProfiler
 * @brief Manages GPU timing using double-buffered queries to avoid stalls.
 */
typedef struct {
	GPUStage stages[MAX_GPU_STAGES];
	int stage_count;

	// Double buffering for queries
	GPUQueryBuffer buffers[GPU_QUERY_BUFFER_COUNT];
	int write_index; /**< Index of the buffer we are currently recording to.
	                  */
	int read_index; /**< Index of the buffer we are reading results from. */

	// State tracking for the current frame
	int current_stage_index;
	int active_stage_indices[MAX_GPU_STAGES];
	int active_stage_count;
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
 */
void gpu_profiler_begin_frame(GPUProfiler* profiler);

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

#endif  // GPU_PROFILER_H
