#include "adaptive_sampler.h"
#include "app_metrics.h"
#include "gpu_profiler.h"
#include "log.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Mock log callback to avoid console I/O
static void noop_log_callback(LogLevel level, const char* tag,
                              const char* message)
{
	(void)level;
	(void)tag;
	(void)message;
}

static void setup_complex_profiler(GPUProfiler* profiler)
{
	memset(profiler, 0, sizeof(GPUProfiler));

	profiler->stage_count = 6;

	// Stage 0: Root
	snprintf(profiler->stages[0].name, sizeof(profiler->stages[0].name),
	         "Root");
	profiler->stages[0].depth = 0;
	profiler->stages[0].parent_index = -1;
	adaptive_sampler_init(&profiler->stages[0].duration_sampler, 2.0F, 60,
	                      60.0F);
	adaptive_sampler_add(&profiler->stages[0].duration_sampler, 16.0f, 1);
	profiler->stages[0].duration_sampler.window_start_time = 0.0;

	// Stage 1: Child 1
	snprintf(profiler->stages[1].name, sizeof(profiler->stages[1].name),
	         "Child 1");
	profiler->stages[1].depth = 1;
	profiler->stages[1].parent_index = 0;
	adaptive_sampler_init(&profiler->stages[1].duration_sampler, 2.0F, 60,
	                      60.0F);
	adaptive_sampler_add(&profiler->stages[1].duration_sampler, 8.0f, 1);

	// Stage 2: Grandchild 1
	snprintf(profiler->stages[2].name, sizeof(profiler->stages[2].name),
	         "Grandchild 1");
	profiler->stages[2].depth = 2;
	profiler->stages[2].parent_index = 1;
	adaptive_sampler_init(&profiler->stages[2].duration_sampler, 2.0F, 60,
	                      60.0F);
	adaptive_sampler_add(&profiler->stages[2].duration_sampler, 4.0f, 1);

	// Stage 3: Grandchild 2
	snprintf(profiler->stages[3].name, sizeof(profiler->stages[3].name),
	         "Grandchild 2");
	profiler->stages[3].depth = 2;
	profiler->stages[3].parent_index = 1;
	adaptive_sampler_init(&profiler->stages[3].duration_sampler, 2.0F, 60,
	                      60.0F);
	adaptive_sampler_add(&profiler->stages[3].duration_sampler, 2.0f, 1);

	// Stage 4: Child 2
	snprintf(profiler->stages[4].name, sizeof(profiler->stages[4].name),
	         "Child 2");
	profiler->stages[4].depth = 1;
	profiler->stages[4].parent_index = 0;
	adaptive_sampler_init(&profiler->stages[4].duration_sampler, 2.0F, 60,
	                      60.0F);
	adaptive_sampler_add(&profiler->stages[4].duration_sampler, 6.0f, 1);

	// Stage 5: Grandchild 3
	snprintf(profiler->stages[5].name, sizeof(profiler->stages[5].name),
	         "Grandchild 3");
	profiler->stages[5].depth = 2;
	profiler->stages[5].parent_index = 4;
	adaptive_sampler_init(&profiler->stages[5].duration_sampler, 2.0F, 60,
	                      60.0F);
	adaptive_sampler_add(&profiler->stages[5].duration_sampler, 3.0f, 1);
}

int main(void)
{
	GPUProfiler profiler;
	setup_complex_profiler(&profiler);

	// Disable logging output to console
	log_set_callback(noop_log_callback);
	// Ensure log level is INFO so formatting happens
	log_set_level(LOG_LEVEL_INFO);

	const int ITERATIONS = 100000;
	clock_t start = clock();

	for (int i = 0; i < ITERATIONS; ++i) {
		// Pass a large current time so window check passes.
		// GPU_PROFILER_WINDOW_DURATION_S is typically 0.5 or 1.0.
		app_metrics_log_gpu_stats(&profiler, 1000.0 + i, true);
	}

	clock_t end = clock();
	double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

	printf("Time taken: %f seconds for %d iterations\n", cpu_time_used,
	       ITERATIONS);

	// Cleanup
	for (int i = 0; i < profiler.stage_count; ++i) {
		adaptive_sampler_cleanup(&profiler.stages[i].duration_sampler);
	}

	return 0;
}
