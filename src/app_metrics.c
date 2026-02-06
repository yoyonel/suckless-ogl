#include "app_metrics.h"

#include "adaptive_sampler.h"
#include "gpu_profiler.h"
#include "log.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

static void format_missed_frames(const AdaptiveSampler* sampler, char* buffer,
                                 size_t size)
{
	uint64_t start_frame = 0;
	uint64_t end_frame = 0;
	adaptive_sampler_get_window_range(sampler, &start_frame, &end_frame);
	size_t count = adaptive_sampler_get_sample_count(sampler);

	if (start_frame > 0 && end_frame >= start_frame) {
		uint64_t total_frames = end_frame - start_frame + 1;
		size_t missed_count = 0;
		if (total_frames > count) {
			missed_count = total_frames - count;
		}

		float miss_rate = 0.0F;
		if (total_frames > 0) {
			miss_rate =
			    ((float)missed_count / (float)total_frames) *
			    100.0F;
		}

		(void)safe_snprintf(buffer, size,
		                    "[Frames: %lu-%lu, Miss: %zu/%lu (%.1f%%)]",
		                    start_frame, end_frame, missed_count,
		                    total_frames, miss_rate);
	} else {
		(void)safe_snprintf(buffer, size, "[Range Empty]");
	}
}

static void log_gpu_stage(const char* stage_name, float avg_ms,
                          const char* details)
{
	if (strcmp(stage_name, "Total Frame") == 0) {
		LOG_INFO("perf.gpu",
		         "Average GPU Frame Time (last 2s): %.3f ms %s", avg_ms,
		         details);
	} else if (strcmp(stage_name, "Auto Exposure") == 0) {
		LOG_INFO("perf.gpu",
		         "Average GPU Auto-Exposure Time (last 2s): %.4f ms %s",
		         avg_ms, details);
	} else if (strcmp(stage_name, "Bloom") == 0) {
		LOG_INFO("perf.gpu",
		         "Average GPU Bloom Time (last 2s): %.4f ms %s", avg_ms,
		         details);
	} else if (strcmp(stage_name, "EnvMap") == 0) {
		LOG_INFO("perf.gpu",
		         "Average GPU EnvMap Time (last 2s): %.4f ms %s",
		         avg_ms, details);
	} else if (strcmp(stage_name, "Spheres") == 0) {
		LOG_INFO("perf.gpu",
		         "Average GPU Spheres Rendering Time (last 2s): %.4f "
		         "ms %s",
		         avg_ms, details);
	}
}

void app_metrics_log_gpu_stats(GPUProfiler* profiler, double current_time)
{
	if (!profiler) {
		return;
	}

	// On boucle sur toutes les étapes enregistrées par le profiler durant
	// cette frame
	for (int i = 0; i < profiler->stage_count; i++) {
		GPUStage* stage = &profiler->stages[i];
		AdaptiveSampler* sampler = &stage->sampler;

		// On vérifie si la fenêtre de 2 secondes est écoulée pour cette
		// étape spécifique
		if (current_time - sampler->window_start_time >=
		    GPU_PROFILER_TOTAL_FRAME_WINDOW_LENGTH) {
			float avg_ms = adaptive_sampler_get_average(sampler);

			if (avg_ms > 0) {
				static const size_t LOG_BUFFER_SIZE = 256;
				char indices_str[LOG_BUFFER_SIZE];
				format_missed_frames(sampler, indices_str,
				                     LOG_BUFFER_SIZE);
				log_gpu_stage(stage->name, avg_ms, indices_str);
			}

			// Reset du sampler de l'étape pour la prochaine fenêtre
			// de 2s
			adaptive_sampler_reset(sampler, current_time);
		}
	}
}
