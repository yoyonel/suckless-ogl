#include "gpu_profiler_ui.h"

#include "adaptive_sampler.h"
#include "app_metrics.h"
#include "gpu_profiler.h"
#include "gpu_profiler_ui_layout.h" /* Private: layout constants */
#include "ui.h"
#include "utils.h"
#include <cglm/types.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* --- Local Helpers --- */
static void hex_to_vec3(uint32_t color, vec3 out)
{
	static const uint32_t SHIFT_R = 16;
	static const uint32_t SHIFT_G = 8;
	static const uint32_t MASK_BYTE = 0xFF;
	static const float COLOR_NORM = 255.0F;

	out[0] = (float)((color >> SHIFT_R) & MASK_BYTE) / COLOR_NORM;
	out[1] = (float)((color >> SHIFT_G) & MASK_BYTE) / COLOR_NORM;
	out[2] = (float)(color & MASK_BYTE) / COLOR_NORM;
}

static void gpu_profiler_ui_update_visibility(GPUProfilerUI* profiler_ui,
                                              float alpha_step)
{
	if (profiler_ui->visible) {
		profiler_ui->global_alpha += alpha_step;
	} else {
		profiler_ui->global_alpha -= alpha_step;
	}

	if (profiler_ui->global_alpha > 1.0F) {
		profiler_ui->global_alpha = 1.0F;
	}
	if (profiler_ui->global_alpha < 0.0F) {
		profiler_ui->global_alpha = 0.0F;
	}
}

/**
 * @brief Safely moves contents of one stage to another, avoiding shallow copies
 * of heap-allocated buffers in AdaptiveSamplers.
 */
static void gpu_stage_move(GPUStage* dest, GPUStage* src)
{
	if (dest == src) {
		return;
	}

	/* 1. Copy non-pointer fields */
	safe_strncpy(dest->name, sizeof(dest->name), src->name,
	             sizeof(dest->name) - 1);
	dest->color = src->color;
	dest->start_offset_ms = src->start_offset_ms;
	dest->duration_ms = src->duration_ms;
	dest->prev_start_offset_ms = src->prev_start_offset_ms;
	dest->prev_duration_ms = src->prev_duration_ms;
	dest->alpha = src->alpha;
	dest->prev_alpha = src->prev_alpha;
	dest->depth = src->depth;
	dest->parent_index = src->parent_index;

	/* 2. Swap AdaptiveSampler buffers to maintain pointer uniqueness */
	AdaptiveSampleItem* tmp_duration = dest->duration_sampler.samples;
	dest->duration_sampler = src->duration_sampler;
	src->duration_sampler.samples = tmp_duration;

	AdaptiveSampleItem* tmp_offset = dest->offset_sampler.samples;
	dest->offset_sampler = src->offset_sampler;
	src->offset_sampler.samples = tmp_offset;
}

static void gpu_profiler_ui_compact_stages(GPUProfilerUI* profiler_ui)
{
	int read_ptr = 0;
	int write_ptr = 0;
	while (read_ptr < profiler_ui->display_profiler.stage_count) {
		GPUStage* stage =
		    &profiler_ui->display_profiler.stages[read_ptr];
		if (stage->alpha < FADE_EPSILON &&
		    stage->prev_alpha < FADE_EPSILON) {
			read_ptr++;
		} else {
			if (write_ptr != read_ptr) {
				gpu_stage_move(&profiler_ui->display_profiler
				                    .stages[write_ptr],
				               stage);
			}
			write_ptr++;
			read_ptr++;
		}
	}
	profiler_ui->display_profiler.stage_count = write_ptr;
}

static void gpu_profiler_ui_sync_stage(GPUProfilerUI* profiler_ui,
                                       const GPUStage* live_stage,
                                       bool* display_updated)
{
	int dest_idx = -1;
	for (int j = 0; j < profiler_ui->display_profiler.stage_count; j++) {
		if (strcmp(profiler_ui->display_profiler.stages[j].name,
		           live_stage->name) == 0) {
			dest_idx = j;
			break;
		}
	}

	if (dest_idx == -1 &&
	    profiler_ui->display_profiler.stage_count < MAX_GPU_STAGES) {
		dest_idx = profiler_ui->display_profiler.stage_count++;
		GPUStage* new_stage =
		    &profiler_ui->display_profiler.stages[dest_idx];

		/* Fix: Do NOT zero-init the whole struct, it overwrites
		 * AdaptiveSamplers */
		safe_strncpy(new_stage->name, sizeof(new_stage->name),
		             live_stage->name, sizeof(new_stage->name) - 1);
		new_stage->alpha = 0.0F;
		new_stage->prev_alpha = 0.0F;
		new_stage->start_offset_ms =
		    adaptive_sampler_get_average(&live_stage->offset_sampler);
		new_stage->duration_ms =
		    adaptive_sampler_get_average(&live_stage->duration_sampler);
		new_stage->prev_start_offset_ms = new_stage->start_offset_ms;
		new_stage->prev_duration_ms = new_stage->duration_ms;
	}

	if (dest_idx != -1) {
		GPUStage* dest_stage =
		    &profiler_ui->display_profiler.stages[dest_idx];
		display_updated[dest_idx] = true;

		dest_stage->prev_start_offset_ms = dest_stage->start_offset_ms;
		dest_stage->prev_duration_ms = dest_stage->duration_ms;
		dest_stage->prev_alpha = dest_stage->alpha;

		dest_stage->start_offset_ms =
		    adaptive_sampler_get_average(&live_stage->offset_sampler);
		dest_stage->duration_ms =
		    adaptive_sampler_get_average(&live_stage->duration_sampler);
		dest_stage->alpha = 1.0F;
		dest_stage->color = live_stage->color;
		dest_stage->depth = live_stage->depth;
		dest_stage->parent_index = live_stage->parent_index;
	}
}

void gpu_profiler_ui_init(GPUProfilerUI* profiler_ui)
{
	*profiler_ui = (GPUProfilerUI){0};
	gpu_profiler_init(&profiler_ui->display_profiler);
	profiler_ui->position = 0;
	profiler_ui->global_alpha = 0.0F;
	profiler_ui->visible = false;
}

void gpu_profiler_ui_update(GPUProfilerUI* profiler_ui,
                            GPUProfiler* live_profiler, double delta_time,
                            double current_time, bool should_log_metrics)
{
	float transition_duration = fminf(GPU_PROFILER_WINDOW_TRANSITION_S,
	                                  GPU_PROFILER_WINDOW_DURATION_S);
	float alpha_step = (float)delta_time / transition_duration;

	gpu_profiler_ui_update_visibility(profiler_ui, alpha_step);

	profiler_ui->display_profiler.transition_progress +=
	    (float)delta_time / transition_duration;
	if (profiler_ui->display_profiler.transition_progress > 1.0F) {
		profiler_ui->display_profiler.transition_progress = 1.0F;
	}

	if (app_metrics_log_gpu_stats(live_profiler, current_time,
	                              should_log_metrics)) {
		bool display_updated[MAX_GPU_STAGES] = {false};

		gpu_profiler_ui_compact_stages(profiler_ui);

		for (int i = 0; i < live_profiler->stage_count; i++) {
			gpu_profiler_ui_sync_stage(profiler_ui,
			                           &live_profiler->stages[i],
			                           display_updated);
		}

		for (int i = 0; i < profiler_ui->display_profiler.stage_count;
		     i++) {
			if (!display_updated[i]) {
				GPUStage* stage =
				    &profiler_ui->display_profiler.stages[i];
				stage->prev_alpha = stage->alpha;
				stage->alpha = 0.0F;
				stage->prev_start_offset_ms =
				    stage->start_offset_ms;
				stage->prev_duration_ms = stage->duration_ms;
			}
		}

		int active_count = 0;
		for (int i = 0; i < profiler_ui->display_profiler.stage_count;
		     i++) {
			if (profiler_ui->display_profiler.stages[i].alpha >
			    VISIBILITY_THRESHOLD) {
				active_count++;
			}
		}

		profiler_ui->prev_box_height = profiler_ui->box_height;
		profiler_ui->box_height =
		    (float)active_count * GPU_PROFILER_ROW_HEIGHT;
		profiler_ui->display_profiler.transition_progress = 0.0F;

		gpu_profiler_reset_samplers(live_profiler, current_time);
	}
}

void gpu_profiler_ui_draw(GPUProfilerUI* profiler_ui, UIContext* ctx,
                          int screen_width, int screen_height)
{
	if (profiler_ui->global_alpha < MIN_VISIBLE_ALPHA_GLOBAL) {
		return;
	}
	if (profiler_ui->display_profiler.stage_count == 0) {
		return;
	}

	float anim_progress = profiler_ui->display_profiler.transition_progress;
	const GPUStage* root_stage = &profiler_ui->display_profiler.stages[0];
	float total_ms =
	    root_stage->prev_duration_ms +
	    ((root_stage->duration_ms - root_stage->prev_duration_ms) *
	     anim_progress);

	if (total_ms <= MIN_MS) {
		return;
	}

	const float original_font_size = ctx->font_size;
	ctx->font_size = GRAPH_FONT_SIZE;

	float total_width = (float)screen_width - (PAD_SIDE * ROW_PAD_DOUBLE);
	float graph_width = total_width * GRAPH_WIDTH_RATIO;
	float text_start_x = PAD_SIDE + graph_width + TEXT_GAP;

	float total_list_height =
	    profiler_ui->prev_box_height +
	    ((profiler_ui->box_height - profiler_ui->prev_box_height) *
	     anim_progress);

	float start_pos_y = 0.0F;
	if (profiler_ui->position == 0) {
		start_pos_y = MARGIN_Y;
	} else {
		start_pos_y =
		    (float)screen_height - total_list_height - MARGIN_Y;
	}

	const vec3 bg_col = {BG_RGB_VAL, BG_RGB_VAL, BG_RGB_VAL};
	ui_draw_rounded_rect(ctx, PAD_SIDE - BG_PAD, start_pos_y - BG_PAD,
	                     total_width + BG_WIDTH_EXT,
	                     total_list_height + BG_WIDTH_EXT, BG_RADIUS,
	                     bg_col, BG_ALPHA * profiler_ui->global_alpha,
	                     screen_width, screen_height);

	for (int i = 0; i < profiler_ui->display_profiler.stage_count; ++i) {
		const GPUStage* stage =
		    &profiler_ui->display_profiler.stages[i];
		float anim_alpha =
		    (stage->prev_alpha +
		     ((stage->alpha - stage->prev_alpha) * anim_progress)) *
		    profiler_ui->global_alpha;

		if (anim_alpha < MIN_VISIBLE_ALPHA_GLOBAL) {
			continue;
		}

		float row_y =
		    start_pos_y + ((float)i * GPU_PROFILER_ROW_HEIGHT);

		float start_ms =
		    stage->prev_start_offset_ms +
		    ((stage->start_offset_ms - stage->prev_start_offset_ms) *
		     anim_progress);
		float duration_ms =
		    stage->prev_duration_ms +
		    ((stage->duration_ms - stage->prev_duration_ms) *
		     anim_progress);

		float x_ratio = start_ms / total_ms;
		float w_ratio = duration_ms / total_ms;
		if (x_ratio < 0.0F) {
			x_ratio = 0.0F;
		}
		if (x_ratio > 1.0F) {
			x_ratio = 1.0F;
		}
		if (w_ratio < 0.0F) {
			w_ratio = 0.0F;
		}
		if (x_ratio + w_ratio > 1.0F) {
			w_ratio = 1.0F - x_ratio;
		}

		float bar_start_x = PAD_SIDE + (x_ratio * graph_width);
		float bar_width = w_ratio * graph_width;
		if (bar_width < MIN_BAR_WIDTH) {
			bar_width = MIN_BAR_WIDTH;
		}

		vec3 col;
		hex_to_vec3(stage->color, col);
		float bar_h =
		    GPU_PROFILER_ROW_HEIGHT - (ROW_PAD * ROW_PAD_DOUBLE);
		float radius = bar_h * BAR_RADIUS_FACTOR;

		ui_draw_rounded_rect(ctx, bar_start_x, row_y + ROW_PAD,
		                     bar_width, bar_h, radius, col, anim_alpha,
		                     screen_width, screen_height);

		float indent_offset = (float)stage->depth * INDENT_STEP;
		const vec3 text_col = {1.0F, 1.0F, 1.0F};
		float text_y = row_y + TEXT_Y_OFFSET;

		ui_draw_text_ex(
		    ctx, stage->name,
		    text_start_x + indent_offset + TEXT_SHADOW_OFFSET,
		    text_y + TEXT_SHADOW_OFFSET, col, anim_alpha, screen_width,
		    screen_height);
		ui_draw_text_ex(ctx, stage->name, text_start_x + indent_offset,
		                text_y, text_col, anim_alpha, screen_width,
		                screen_height);

		char time_buf[TIME_BUF_SIZE];
		(void)safe_snprintf(time_buf, sizeof(time_buf), "%.4f ms",
		                    (double)duration_ms);

		/* Right-align timings to the far right of the background box */
		float text_width = ui_measure_text(ctx, time_buf);
		float right_boundary_x = PAD_SIDE + total_width - BG_PAD;
		float time_x = right_boundary_x - text_width;

		ui_draw_text_ex(ctx, time_buf, time_x, text_y, text_col,
		                anim_alpha, screen_width, screen_height);
	}

	ctx->font_size = original_font_size;
}

void gpu_profiler_ui_toggle_visibility(GPUProfilerUI* profiler_ui)
{
	if (profiler_ui->visible) {
		profiler_ui->visible = false;
	} else {
		profiler_ui->visible = true;
	}
}

void gpu_profiler_ui_toggle_position(GPUProfilerUI* profiler_ui)
{
	profiler_ui->position = (profiler_ui->position == 0) ? 1 : 0;
}

void gpu_profiler_ui_cleanup(GPUProfilerUI* profiler_ui)
{
	if (profiler_ui) {
		gpu_profiler_cleanup(&profiler_ui->display_profiler);
	}
}
