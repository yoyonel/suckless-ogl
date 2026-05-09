#include "gpu_profiler.h"

void gpu_profiler_init(GPUProfiler* profiler)
{
	(void)profiler;
}

void gpu_profiler_cleanup(GPUProfiler* profiler)
{
	(void)profiler;
}

void gpu_profiler_begin_frame(GPUProfiler* profiler, uint64_t frame_index)
{
	(void)profiler;
	(void)frame_index;
}

void gpu_profiler_set_enabled(GPUProfiler* profiler, bool enabled)
{
	(void)profiler;
	(void)enabled;
}

void gpu_profiler_start_stage(GPUProfiler* profiler, const char* name,
                              uint32_t color)
{
	(void)profiler;
	(void)name;
	(void)color;
}

void gpu_profiler_end_stage(GPUProfiler* profiler)
{
	(void)profiler;
}

void gpu_profiler_reset_samplers(GPUProfiler* profiler, double current_time)
{
	(void)profiler;
	(void)current_time;
}
