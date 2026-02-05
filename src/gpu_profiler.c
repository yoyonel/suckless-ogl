#include "gpu_profiler.h"

#include "adaptive_sampler.h"
#include "glad/glad.h"
#include "perf_timer.h"
#include <stdint.h>
#include <string.h>

void gpu_profiler_init(GPUProfiler* profiler)
{
	if (!profiler) {
		return;
	}
	*profiler = (GPUProfiler){0};
	// Initial indices: 0 for write, 1 for read
	profiler->write_index = 0;
	profiler->read_index = 1;
}

void gpu_profiler_cleanup(GPUProfiler* profiler)
{
	if (!profiler) {
		return;
	}

	// 1. Nettoyer les requêtes OpenGL (déjà présent)
	for (int i = 0; i < GPU_QUERY_BUFFER_COUNT; ++i) {
		for (int j = 0; j < MAX_GPU_STAGES; ++j) {
			gpu_timer_cleanup(&profiler->buffers[i].queries[j]);
		}
	}

	// 2. AJOUT : Nettoyer les buffers de samples pour CHAQUE étape
	// C'est ici que la mémoire allouée par realloc/malloc dans
	// adaptive_sampler_add est libérée
	for (int i = 0; i < MAX_GPU_STAGES; ++i) {
		adaptive_sampler_cleanup(&profiler->stages[i].sampler);
	}
}

void gpu_profiler_begin_frame(GPUProfiler* profiler)
{
	if (!profiler) {
		return;
	}

	// 1. Process results from the READ buffer (previous frame)
	GPUQueryBuffer* read_buf = &profiler->buffers[profiler->read_index];

	uint64_t frame_start_ns = 0;
	int frame_start_set = 0;

	// Constant for conversion (1e-6)
	const double TO_MS = 1.0 / 1000000.0;

	for (int i = 0; i < profiler->stage_count; ++i) {
		GPUTimer* timer = &read_buf->queries[i];

		// Check availability first
		GLint available = 0;
		glGetQueryObjectiv(timer->query_end, GL_QUERY_RESULT_AVAILABLE,
		                   &available);

		if (!available) {
			continue;
		}

		uint64_t start_curr = 0;
		uint64_t end_curr = 0;

		glGetQueryObjectui64v(timer->query_start, GL_QUERY_RESULT,
		                      &start_curr);
		glGetQueryObjectui64v(timer->query_end, GL_QUERY_RESULT,
		                      &end_curr);

		// Set frame start if this is the first valid stage
		if (!frame_start_set) {
			frame_start_ns = start_curr;
			frame_start_set = 1;
		}

		// Calculate duration
		uint64_t duration_ns =
		    (end_curr > start_curr) ? (end_curr - start_curr) : 0;
		double duration_ms = (double)duration_ns * TO_MS;

		// Calculate offset
		uint64_t offset_ns = (start_curr > frame_start_ns)
		                         ? (start_curr - frame_start_ns)
		                         : 0;
		double offset_ms = (double)offset_ns * TO_MS;

		adaptive_sampler_add(&profiler->stages[i].sampler,
		                     (float)duration_ms);
		profiler->stages[i].duration_ms = duration_ms;
		profiler->stages[i].start_offset_ms = offset_ms;
	}

	// 2. Swap indices for the NEW frame
	int temp = profiler->write_index;
	profiler->write_index = profiler->read_index;
	profiler->read_index = temp;

	// 3. Reset stage count for the new write frame
	profiler->stage_count = 0;
	profiler->active_stage_count = 0;
}

void gpu_profiler_start_stage(GPUProfiler* profiler, const char* name,
                              uint32_t color)
{
	if (!profiler || profiler->stage_count >= MAX_GPU_STAGES) {
		return;
	}

	int idx = profiler->stage_count++;
	GPUStage* stage = &profiler->stages[idx];

	// Copy basic info
	size_t max_len = sizeof(stage->name) - 1;
	size_t copy_idx = 0;
	if (name) {
		while (copy_idx < max_len && name[copy_idx] != '\0') {
			stage->name[copy_idx] = name[copy_idx];
			copy_idx++;
		}
	}
	stage->name[copy_idx] = '\0';
	stage->color = color;

	// Start the timer for this stage in the WRITE buffer
	GPUQueryBuffer* buffer = &profiler->buffers[profiler->write_index];
	gpu_timer_start(&buffer->queries[idx]);

	// Track active stage nesting
	if (profiler->active_stage_count < MAX_GPU_STAGES) {
		profiler->active_stage_indices[profiler->active_stage_count++] =
		    idx;
	}
}

void gpu_profiler_end_stage(GPUProfiler* profiler)
{
	if (!profiler || profiler->active_stage_count == 0) {
		return;
	}

	// Retrieve index of current active stage
	int idx =
	    profiler->active_stage_indices[profiler->active_stage_count - 1];

	GPUQueryBuffer* buffer = &profiler->buffers[profiler->write_index];
	gpu_timer_stop(&buffer->queries[idx]);

	profiler->active_stage_count--;
}
