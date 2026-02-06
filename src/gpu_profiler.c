#include "gpu_profiler.h"

#include "adaptive_sampler.h"
#include "glad/glad.h"
#include "utils.h"
#include <stddef.h> /* size_t */

/* Constants to avoid magic numbers */
static const float SAMPLER_WINDOW_DURATION = 2.0F;
static const size_t SAMPLER_TARGET_SAMPLES = 120;
static const float SAMPLER_INITIAL_GUESS_FPS = 60.0F;
static const double NS_TO_MS = 1.0 / 1000000.0;

void gpu_profiler_init(GPUProfiler* profiler)
{
	if (!profiler) {
		return;
	}

	/* 1. Safe Zero Initialization (replaces unsafe memset) */
	*profiler = (GPUProfiler){0};

	/* 2. Init Ping-Pong */
	profiler->write_index = 0;
	profiler->read_index = 1;

	/* 3. Gen Queries */
	for (int i = 0; i < GPU_QUERY_BUFFER_COUNT; ++i) {
		for (int j = 0; j < MAX_GPU_STAGES; ++j) {
			glGenQueries(
			    1, &profiler->buffers[i].queries[j].query_start);
			glGenQueries(
			    1, &profiler->buffers[i].queries[j].query_end);
		}
	}

	/* 4. Init Samplers */
	for (int i = 0; i < MAX_GPU_STAGES; ++i) {
		adaptive_sampler_init(
		    &profiler->stages[i].sampler, SAMPLER_WINDOW_DURATION,
		    SAMPLER_TARGET_SAMPLES, SAMPLER_INITIAL_GUESS_FPS);
	}
}

void gpu_profiler_cleanup(GPUProfiler* profiler)
{
	if (!profiler) {
		return;
	}

	/* 1. Cleanup Queries */
	for (int i = 0; i < GPU_QUERY_BUFFER_COUNT; ++i) {
		for (int j = 0; j < MAX_GPU_STAGES; ++j) {
			GPUTimer* timer = &profiler->buffers[i].queries[j];
			if (timer->query_start) {
				glDeleteQueries(1, &timer->query_start);
				timer->query_start = 0;
			}
			if (timer->query_end) {
				glDeleteQueries(1, &timer->query_end);
				timer->query_end = 0;
			}
		}
	}

	/* 2. Free Samplers */
	for (int i = 0; i < MAX_GPU_STAGES; ++i) {
		adaptive_sampler_cleanup(&profiler->stages[i].sampler);
	}
}

void gpu_profiler_begin_frame(GPUProfiler* profiler)
{
	if (!profiler) {
		return;
	}

	GPUQueryBuffer* read_buf = &profiler->buffers[profiler->read_index];

	uint64_t frame_start_ns = 0;
	int frame_start_set = 0;

	/* 0. Save current frame's stage count to the write buffer */
	profiler->buffers[profiler->write_index].stage_count =
	    profiler->stage_count;

	/* 1. Process previous frame results */
	for (int i = 0; i < read_buf->stage_count; ++i) {
		GPUTimer* timer = &read_buf->queries[i];

		if (timer->query_end == 0) {
			continue;
		}

		GLint available = 0;
		glGetQueryObjectiv(timer->query_end, GL_QUERY_RESULT_AVAILABLE,
		                   &available);

		if (!available) {
			continue;
		}

		uint64_t start = 0;
		uint64_t end = 0;

		glGetQueryObjectui64v(timer->query_start, GL_QUERY_RESULT,
		                      &start);
		glGetQueryObjectui64v(timer->query_end, GL_QUERY_RESULT, &end);

		if (end == 0) {
			continue;
		}

		if (!frame_start_set) {
			frame_start_ns = start;
			frame_start_set = 1;
		}

		uint64_t duration_ns = (end > start) ? (end - start) : 0;
		double duration_ms = (double)duration_ns * NS_TO_MS;

		uint64_t offset_ns =
		    (start > frame_start_ns) ? (start - frame_start_ns) : 0;
		double offset_ms = (double)offset_ns * NS_TO_MS;

		adaptive_sampler_add(&profiler->stages[i].sampler,
		                     (float)duration_ms);
		profiler->stages[i].duration_ms = duration_ms;
		profiler->stages[i].start_offset_ms = offset_ms;
	}

	/* 2. Swap Buffers */
	int temp = profiler->write_index;
	profiler->write_index = profiler->read_index;
	profiler->read_index = temp;

	/* 3. Reset for new frame */
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

	/* Safe manual string copy to replace insecure strncpy */
	safe_strncpy(stage->name, sizeof(stage->name) - 1,
	             name ? name : "Unknown", sizeof(stage->name) - 1);
	stage->color = color;

	if (profiler->active_stage_count < MAX_GPU_STAGES) {
		profiler->active_stage_indices[profiler->active_stage_count++] =
		    idx;
	}

	GPUQueryBuffer* buffer = &profiler->buffers[profiler->write_index];
	glQueryCounter(buffer->queries[idx].query_start, GL_TIMESTAMP);
}

void gpu_profiler_end_stage(GPUProfiler* profiler)
{
	if (!profiler || profiler->active_stage_count == 0) {
		return;
	}

	int idx =
	    profiler->active_stage_indices[profiler->active_stage_count - 1];
	profiler->active_stage_count--;

	GPUQueryBuffer* buffer = &profiler->buffers[profiler->write_index];
	glQueryCounter(buffer->queries[idx].query_end, GL_TIMESTAMP);
}
