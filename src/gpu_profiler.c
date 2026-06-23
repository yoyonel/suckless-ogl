#include "gpu_profiler.h"

#include "adaptive_sampler.h"
#include "glad/glad.h"
#include "metric_stack.h"
#include "profiler.h"
#include "utils.h"
#include <stddef.h> /* size_t */
#include <stdint.h>

#ifdef TRACY_ENABLE
#include "tracy_gpu.h"
#endif

/* Constants to avoid magic numbers */
static const float SAMPLER_WINDOW_DURATION = GPU_PROFILER_WINDOW_DURATION_S;
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

#ifdef TRACY_ENABLE
	tracy_gpu_init();
#endif

	/* 2. Init Ping-Pong */
	profiler->write_index = 0;
	profiler->read_index = 1;

	/* 2.1 Init Hierarchy Stack */
	metric_stack_init(&profiler->hierarchy_stack);

	/* 3. Gen Queries (GL_TIMESTAMP pairs) */
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
		adaptive_sampler_init(&profiler->stages[i].duration_sampler,
		                      SAMPLER_WINDOW_DURATION,
		                      SAMPLER_TARGET_SAMPLES,
		                      SAMPLER_INITIAL_GUESS_FPS);

		adaptive_sampler_init(&profiler->stages[i].offset_sampler,
		                      SAMPLER_WINDOW_DURATION,
		                      SAMPLER_TARGET_SAMPLES,
		                      SAMPLER_INITIAL_GUESS_FPS);
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
		adaptive_sampler_cleanup(&profiler->stages[i].duration_sampler);
		adaptive_sampler_cleanup(&profiler->stages[i].offset_sampler);
	}
}

void gpu_profiler_set_enabled(GPUProfiler* profiler, bool enabled)
{
	if (profiler) {
		profiler->enabled = enabled;
	}
}

void gpu_profiler_begin_frame(GPUProfiler* profiler, uint64_t frame_index)
{
	if (!profiler) {
		return;
	}

	/* If disabled, just reset the recording counter so we don't overflow
	 * if start_stage is called (though start_stage should also check
	 * enabled). We do NOT swap buffers because we aren't generating new
	 * data. */
	if (!profiler->enabled) {
		profiler->recording_count = 0;
		metric_stack_init(&profiler->hierarchy_stack);
		return;
	}

	GPUQueryBuffer* read_buf = &profiler->buffers[profiler->read_index];

	uint64_t frame_start_ns = 0;
	int frame_start_set = 0;

	/* 0. Save current frame's recording count to the write buffer */
	profiler->buffers[profiler->write_index].stage_count =
	    profiler->recording_count;
	profiler->buffers[profiler->write_index].frame_index = frame_index;

	/* 1. Process previous frame results using GL_TIMESTAMP pairs.
	 * Duration = end_timestamp - start_timestamp.
	 *
	 * On Intel Iris Xe (Mesa), compute shader stages may show near-zero
	 * durations because the driver aggressively pipelines compute
	 * dispatches.  This is expected — it reveals that compute work
	 * overlaps with preceding/following GPU work rather than blocking
	 * the pipeline.  The timestamps accurately reflect the driver's
	 * scheduling behavior. */
	PROFILE_ZONE(query_readback_ctx, "GPU Query Readback (sync)");
	for (int i = 0; i < read_buf->stage_count; ++i) {
		GPUTimer* timer = &read_buf->queries[i];

		if (timer->query_end == 0) {
			continue;
		}

		uint64_t start = 0;
		uint64_t end = 0;

		/* Force wait for result to avoid dropping frames/overlay
		 * flickering. If the GPU is lagging, we stall here, which is
		 * better than losing profiling data and overwriting the buffer.
		 */
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

		/* Restore stage metadata from the buffer that recorded these
		 * queries, ensuring names/colors match the correct frame */
		GPUStageInfo* info = &read_buf->stage_info[i];
		safe_strncpy(profiler->stages[i].name,
		             sizeof(profiler->stages[i].name) - 1, info->name,
		             sizeof(profiler->stages[i].name) - 1);
		profiler->stages[i].color = info->color;
		profiler->stages[i].depth = info->depth;
		profiler->stages[i].parent_index = info->parent_index;

		adaptive_sampler_add(&profiler->stages[i].duration_sampler,
		                     (float)duration_ms, read_buf->frame_index);
		adaptive_sampler_add(&profiler->stages[i].offset_sampler,
		                     (float)offset_ms, read_buf->frame_index);
		profiler->stages[i].duration_ms = (float)duration_ms;
		profiler->stages[i].start_offset_ms = (float)offset_ms;
	}

	PROFILE_ZONE_END(query_readback_ctx);

	/* Update display stage_count from last completed frame's read-back.
	 * This is what the UI iterates over — it must NOT be reset to 0. */
	profiler->stage_count = read_buf->stage_count;

	/* 2. Swap Buffers */
	int temp = profiler->write_index;
	profiler->write_index = profiler->read_index;
	profiler->read_index = temp;

	/* 3. Reset recording counter for new frame (stage_count stays for UI)
	 */
	profiler->recording_count = 0;
	metric_stack_init(&profiler->hierarchy_stack);
}

void gpu_profiler_start_stage(GPUProfiler* profiler, const char* name,
                              uint32_t color)
{
	if (!profiler || !profiler->enabled ||
	    profiler->recording_count >= MAX_GPU_STAGES) {
		return;
	}

	int idx = profiler->recording_count++;

	/* Compute hierarchy info from the recording stack */
	int depth = metric_stack_get_depth(&profiler->hierarchy_stack);
	int parent_index = metric_stack_peek(&profiler->hierarchy_stack);

	if (!metric_stack_push(&profiler->hierarchy_stack, idx)) {
		/* Stack overflow safety */
		return;
	}

	/* Write metadata ONLY to the per-frame buffer (not to stages[]).
	 * stages[] is the display array, updated exclusively during
	 * begin_frame read-back to avoid index conflicts between frames. */
	GPUQueryBuffer* buffer = &profiler->buffers[profiler->write_index];
	GPUStageInfo* info = &buffer->stage_info[idx];
	safe_strncpy(info->name, sizeof(info->name) - 1,
	             name ? name : "Unknown", sizeof(info->name) - 1);
	info->color = color;
	info->depth = depth;
	info->parent_index = parent_index;

	glQueryCounter(buffer->queries[idx].query_start, GL_TIMESTAMP);
}

void gpu_profiler_end_stage(GPUProfiler* profiler)
{
	if (!profiler || !profiler->enabled) {
		return;
	}

	int idx = metric_stack_pop(&profiler->hierarchy_stack);
	if (idx == -1) {
		/* Stack underflow, or no active stage */
		return;
	}

	GPUQueryBuffer* buffer = &profiler->buffers[profiler->write_index];
	glQueryCounter(buffer->queries[idx].query_end, GL_TIMESTAMP);
}

void gpu_profiler_reset_samplers(GPUProfiler* profiler, double current_time)
{
	if (!profiler) {
		return;
	}

	for (int i = 0; i < MAX_GPU_STAGES; ++i) {
		adaptive_sampler_reset(&profiler->stages[i].duration_sampler,
		                       current_time);
		adaptive_sampler_reset(&profiler->stages[i].offset_sampler,
		                       current_time);
	}
}
