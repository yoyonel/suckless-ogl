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

static void format_relative_percentages(const GPUProfiler* profiler,
                                        int stage_index, float current_avg_ms,
                                        char* buffer, size_t size)
{
	// Safe initialization
	buffer[0] = '\0';

	// Traverse up the hierarchy
	int parent_idx = profiler->stages[stage_index].parent_index;
	bool first = true;

	while (parent_idx != -1) {
		const GPUStage* parent = &profiler->stages[parent_idx];
		float parent_avg_ms =
		    adaptive_sampler_get_average(&parent->duration_sampler);

		static const float MIN_DURATION_THRESHOLD_MS = 0.0001F;
		if (parent_avg_ms > MIN_DURATION_THRESHOLD_MS) {
			float percent =
			    (current_avg_ms / parent_avg_ms) * 100.0F;
			if (percent > 100.0F) {
				percent = 100.0F;
			}

			if (first) {
				safe_strncat(buffer, size, " {");
			} else {
				safe_strncat(buffer, size, ", ");
			}

			// MAX_PERCENT_STR_SIZE for "XX% ParentName"
			const size_t MAX_PERCENT_STR_SIZE = 64;
			char temp[MAX_PERCENT_STR_SIZE];
			(void)safe_snprintf(temp, sizeof(temp), "%.0f%% %s",
			                    percent, parent->name);
			safe_strncat(buffer, size, temp);
			first = false;
		}

		parent_idx = parent->parent_index;
	}

	if (!first) {
		safe_strncat(buffer, size, "}");
	}
}

static void build_ascii_bar(char* bar, size_t bar_size, int start_char,
                            int width_char, char symbol, bool is_root)
{
	const int BAR_WIDTH = 80;
	if (bar_size <= (size_t)BAR_WIDTH) {
		return;
	}

	// Fill with spaces
	for (int j = 0; j < BAR_WIDTH; ++j) {
		bar[j] = ' ';
	}
	bar[BAR_WIDTH] = '\0';

	// Special handling for Root: brackets
	if (is_root) {
		if (width_char >= 2) {
			bar[start_char] = '[';
			bar[start_char + width_char - 1] = ']';
			for (int k = start_char + 1;
			     k < start_char + width_char - 1; ++k) {
				bar[k] = '-';
			}
		} else {
			bar[start_char] = '|';
		}
	} else {
		// Normal stage
		for (int k = start_char; k < start_char + width_char; ++k) {
			bar[k] = symbol;
		}
	}
}

static void log_ascii_timeline(const GPUProfiler* profiler)
{
	if (!profiler || profiler->stage_count == 0) {
		return;
	}

	// Find total duration (frame time) for normalization
	// Assuming the first stage (index 0) is the root/Total Frame
	float total_ms = (float)profiler->stages[0].duration_ms;
	static const float MIN_TOTAL_MS = 0.0001F;
	if (total_ms <= MIN_TOTAL_MS) {
		return;  // Avoid division by zero
	}

	const int BAR_WIDTH = 80;
	// Symbols palette
	const char symbols[] = "xo*=-+@#";
	const int num_symbols = (int)(sizeof(symbols) - 1);

	static const size_t BUFFER_SIZE = 4096;
	char buffer[BUFFER_SIZE];
	buffer[0] = '\0';

	// Start with a newline to separate from previous logs
	safe_strncat(buffer, BUFFER_SIZE, "\n");

	for (int i = 0; i < profiler->stage_count; ++i) {
		const GPUStage* stage = &profiler->stages[i];

		float start_ratio = (float)stage->start_offset_ms / total_ms;
		float width_ratio = (float)stage->duration_ms / total_ms;

		if (start_ratio < 0.0F) {
			start_ratio = 0.0F;
		}
		if (start_ratio > 1.0F) {
			start_ratio = 1.0F;
		}
		if (width_ratio < 0.0F) {
			width_ratio = 0.0F;
		}
		if (start_ratio + width_ratio > 1.0F) {
			width_ratio = 1.0F - start_ratio;
		}

		int start_char = (int)(start_ratio * (float)BAR_WIDTH);
		int width_char = (int)(width_ratio * (float)BAR_WIDTH);

		if (width_char < 1 && width_ratio > 0.0F) {
			width_char = 1;  // Ensure small items are visible
		}
		if (start_char + width_char > BAR_WIDTH) {
			width_char = BAR_WIDTH - start_char;
		}

		// Build the bar string
		static const size_t BAR_BUFFER_SIZE = 128;
		char bar[BAR_BUFFER_SIZE];
		build_ascii_bar(bar, BAR_BUFFER_SIZE, start_char, width_char,
		                symbols[i % num_symbols], stage->depth == 0);

		static const int NAME_INDENT_SIZE = 64;
		char indent_name[NAME_INDENT_SIZE];
		indent_name[0] = '\0';
		int indent_level = stage->depth * 4;  // 4 spaces per level
		if (indent_level >= NAME_INDENT_SIZE - 1) {
			indent_level = NAME_INDENT_SIZE - 1;
		}
		for (int k = 0; k < indent_level; ++k) {
			indent_name[k] = ' ';
		}
		indent_name[indent_level] = '\0';

		static const size_t MAX_LINE_SIZE = 256;
		char line[MAX_LINE_SIZE];
		(void)safe_snprintf(line, sizeof(line), " %s %s%s (%.4f ms)\n",
		                    bar, indent_name, stage->name,
		                    stage->duration_ms);
		safe_strncat(buffer, BUFFER_SIZE, line);
	}

	LOG_INFO("perf.gpu", "%s", buffer);
}

static void log_gpu_stage(const char* stage_name, float avg_ms,
                          const char* details, const char* percentages,
                          int depth)

{
	const int MAX_INDENT_SIZE = 64;
	char indent[MAX_INDENT_SIZE];
	// Safe initialization
	indent[0] = '\0';

	int indent_level = depth * 2;
	if (indent_level >= MAX_INDENT_SIZE) {
		indent_level = MAX_INDENT_SIZE - 1;
	}

	for (int i = 0; i < indent_level; ++i) {
		indent[i] = ' ';
	}
	indent[indent_level] = '\0';

	LOG_INFO("perf.gpu", "%s%s Time (last %.0fs): %.4f ms %s%s", indent,
	         stage_name, (double)GPU_PROFILER_WINDOW_DURATION_S, avg_ms,
	         details, percentages);
}

bool app_metrics_log_gpu_stats(GPUProfiler* profiler, double current_time,
                               bool should_log)
{
	if (!profiler) {
		return false;
	}

	// Check if Root (Total Frame) is ready to report
	// We assume Index 0 is always the Root
	if (profiler->stage_count == 0) {
		return false;
	}

	GPUStage* root_stage = &profiler->stages[0];
	AdaptiveSampler* root_sampler = &root_stage->duration_sampler;

	if (current_time - root_sampler->window_start_time <
	    GPU_PROFILER_WINDOW_DURATION_S) {
		return false;
	}

	/* If no samples were collected (e.g., during window resize or heavy
	 * lag), do NOT update the UI with "0 ms". Instead, keep the previous
	 * valid state visible indefinitely until we get new samples. */
	if (root_sampler->count == 0) {
		return false;
	}

	if (should_log) {
		// Pass 1: Logging (All stages at once)
		for (int i = 0; i < profiler->stage_count; i++) {
			GPUStage* stage = &profiler->stages[i];
			AdaptiveSampler* sampler = &stage->duration_sampler;

			float avg_ms = adaptive_sampler_get_average(sampler);

			if (avg_ms > 0) {
				static const size_t LOG_BUFFER_SIZE = 256;
				char indices_str[LOG_BUFFER_SIZE];
				format_missed_frames(sampler, indices_str,
				                     LOG_BUFFER_SIZE);

				char percentages_str[LOG_BUFFER_SIZE];
				format_relative_percentages(profiler, i, avg_ms,
				                            percentages_str,
				                            LOG_BUFFER_SIZE);

				log_gpu_stage(stage->name, avg_ms, indices_str,
				              percentages_str, stage->depth);
			}
		}

		// Log ASCII Timeline immediately after text logs
		log_ascii_timeline(profiler);
	}

	return true;
}
