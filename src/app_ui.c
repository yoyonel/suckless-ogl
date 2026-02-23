#include "app_ui.h"

#include "action_notifier.h"
#include "adaptive_sampler.h"
#include "app.h"
#include "app_settings.h"
#include "glad/glad.h"
#include "postprocess.h"
#include "render_utils.h"
#include "ui.h"
#include "utils.h"
#ifdef TRACY_ENABLE
#include "../deps/tracy/public/tracy/TracyC.h"
#endif
#include <GLFW/glfw3.h>
#include <cglm/types.h>
#include <cglm/vec3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	DEBUG_TEXT_BUFFER_SIZE = 128,
	RANGE_TEXT_BUFFER_SIZE = 64,
	ENV_TEXT_BUFFER_SIZE = 256,
	EXPOSURE_TEXT_BUFFER_SIZE = 64
};

static const float GRAPH_TEXT_PADDING = 20.0F;
static const vec3 GRAPH_TEXT_COLOR = {0.8F, 0.8F, 0.8F};
static const float LUMINANCE_EPSILON = 0.0001F;
static const float DEBUG_TEXT_Y_OFFSET = DEFAULT_FONT_SIZE * 4.0F;
static const vec3 DEBUG_ORANGE_COLOR = {1.0F, 0.5F, 0.0F};
static const vec3 HISTO_BAR_COLOR_GREEN = {0.0F, 0.7F, 0.0F};
static const vec3 HISTO_BAR_COLOR_BLUE = {0.0F, 0.5F, 0.8F};
static const vec3 HISTO_BAR_COLOR_RED = {0.8F, 0.5F, 0.0F};
static const vec3 ENV_TEXT_COLOR = {0.7F, 0.7F, 0.7F};

/* UI Animation Constants */
static const double UI_SPINNER_SPEED = 10.0;
static const float UI_LOADING_TEXT_WIDTH_FACTOR = 20.0F;
static const float UI_SPINNER_SIZE = 64.0F;
static const float UI_CENTER_FACTOR = 0.5F;
static const float UI_TEXT_OFFSET_FACTOR = 0.8F;
static const vec3 UI_SPINNER_COLOR = {90.0F / 255.0F, 111.0F / 255.0F,
                                      185.0F / 255.0F};
static const size_t UI_LOADING_TEXT_SIZE = 64;

void app_draw_help_overlay(App* app)
{
	const GLStateBackup saved_state = render_utils_save_state();
	render_utils_setup_ui_state();

	static const float HELP_START_X = 20.0F;
	static const float HELP_START_Y = 60.0F;
	static const float HELP_PADDING = 5.0F;
	static const float HELP_SECTION_PADDING = 10.0F;
	static const vec3 HELP_COLOR = {0.1F, 1.0F, 0.25F};

	UILayout layout;
	ui_layout_init(&layout, &app->ui, HELP_START_X, HELP_START_Y,
	               HELP_PADDING, app->width, app->height);

	/* Section: Controls */
	ui_layout_text(&layout, "--- Controls ---", HELP_COLOR);
	ui_layout_text(&layout, "[WASD] Move", HELP_COLOR);
	ui_layout_text(&layout, "[Mouse] Look", HELP_COLOR);
	ui_layout_text(&layout, "[Scroll] Speed/Zoom", HELP_COLOR);
	ui_layout_text(&layout, "[Left Click] Capture Mouse", HELP_COLOR);
	ui_layout_text(&layout, "[ESC] Release Mouse/Exit", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: Features */
	ui_layout_text(&layout, "--- Features ---", HELP_COLOR);
	ui_layout_text(&layout, "[F1] Cycle Text Overlays", HELP_COLOR);
	ui_layout_text(&layout, "[F2] Toggle Help", HELP_COLOR);
	ui_layout_text(&layout, "[F] Toggle Flashlight", HELP_COLOR);
	ui_layout_text(&layout, "[Z] Toggle Wireframe", HELP_COLOR);
	ui_layout_text(&layout, "[H] Toggle UI/Help", HELP_COLOR);
	ui_layout_text(&layout, "[J] Toggle Auto-Exposure", HELP_COLOR);
	ui_layout_text(&layout, "[B] Toggle Bloom", HELP_COLOR);
	ui_layout_text(&layout, "[M] Toggle Motion Blur", HELP_COLOR);
	ui_layout_text(&layout, "[L] Toggle Billboard Mode", HELP_COLOR);
	ui_layout_text(&layout, "[K] Toggle Envmap", HELP_COLOR);
	ui_layout_text(&layout, "[T] Toggle Transition Mode", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: Environment */
	ui_layout_text(&layout, "--- Environment ---", HELP_COLOR);
	ui_layout_text(&layout, "[PgUp/PgDn] Change HDR", HELP_COLOR);
	ui_layout_text(&layout, "[Shift + PgUp/PgDn] Blur HDR", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: Post-Process Styles */
	ui_layout_text(&layout, "--- Styles (Numpad) ---", HELP_COLOR);
	ui_layout_text(&layout, "[1] Default (Clean)", HELP_COLOR);
	ui_layout_text(&layout, "[2] Subtle", HELP_COLOR);
	ui_layout_text(&layout, "[3] Cinematic", HELP_COLOR);
	ui_layout_text(&layout, "[4] Vintage", HELP_COLOR);
	ui_layout_text(&layout, "[5] Matrix", HELP_COLOR);
	ui_layout_text(&layout, "[6] BW Contrast", HELP_COLOR);
	ui_layout_text(&layout, "[0] Reset", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: System */
	ui_layout_text(&layout, "--- System ---", HELP_COLOR);
	ui_layout_text(&layout, "[F9] Toggle Performance Mode", HELP_COLOR);
	ui_layout_text(&layout, "[F12] Take Screenshot", HELP_COLOR);

	render_utils_restore_state(&saved_state);
}

void draw_exposure_debug_text(App* app)
{
	float exposure_val = 0.0F;
#ifdef TRACY_ENABLE
	TracyCZoneN(ctx, "AE Sync Readback (glGetTexImage)", 1);
#endif
	glBindTexture(GL_TEXTURE_2D,
	              app->postprocess.auto_exposure_fx.exposure_tex);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, &exposure_val);
	glBindTexture(GL_TEXTURE_2D, 0);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(ctx);
#endif

	char debug_text[DEBUG_TEXT_BUFFER_SIZE];
	float luminance =
	    (exposure_val > LUMINANCE_EPSILON) ? (1.0F / exposure_val) : 0.0F;
	(void)safe_snprintf(debug_text, sizeof(debug_text),
	                    "Auto Exposure: %.4f | Scene "
	                    "Lum: %.4f",
	                    exposure_val, luminance);

	ui_draw_text(&app->ui, debug_text, DEFAULT_FONT_OFFSET_X,
	             DEFAULT_FONT_OFFSET_Y + DEBUG_TEXT_Y_OFFSET,
	             (float*)DEBUG_ORANGE_COLOR, app->width, app->height);
}

int compute_luminance_histogram(App* app, int* buckets, int size,
                                float* min_lum, float* max_lum)
{
	/* Initialize buckets */
	for (int i = 0; i < size; i++) {
		buckets[i] = 0;
	}

	static const float HISTO_MIN_INIT = 1000.0F;
	static const float HISTO_MAX_INIT = -1000.0F;
	*min_lum = HISTO_MIN_INIT;
	*max_lum = HISTO_MAX_INIT;

	/* Bind PBO and map buffer (Read Previous Frame) */
	glBindBuffer(GL_PIXEL_PACK_BUFFER, app->histogram_pbo);
	float* lum_data = NULL;
	lum_data = (float*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

	int processed = 0;
	if (lum_data) {
#ifdef TRACY_ENABLE
		TracyCZoneN(ctx, "Histogram Map & Process", 1);
#endif
		for (int i = 0; i < LUM_HISTOGRAM_SIZE; i++) {
			float val = lum_data[i];
			if (val < *min_lum) {
				*min_lum = val;
			}
			if (val > *max_lum) {
				*max_lum = val;
			}

			static const float RANGE_OFFSET = 5.0F;
			static const float RANGE_SCALE = 10.0F;
			float norm = (val + RANGE_OFFSET) / RANGE_SCALE;
			int idx = (int)(norm * (float)size);
			if (idx < 0) {
				idx = 0;
			}
			if (idx >= size) {
				idx = size - 1;
			}

			buckets[idx]++;
		}
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
#ifdef TRACY_ENABLE
		TracyCZoneEnd(ctx);
#endif
		processed = 1;
	}

	/* Trigger Async Transfer (For Next Frame) */
	glBindTexture(GL_TEXTURE_2D,
	              app->postprocess.auto_exposure_fx.downsample_tex);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, 0); /* Offset 0 */
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	return processed;
}

void draw_luminance_histogram_graph(App* app, const int* buckets, int size,
                                    float min_lum, float max_lum)
{
	static const float GRAPH_POS_X = 20.0F;
	static const float GRAPH_POS_Y_OFF = 200.0F;
	static const float GRAPH_DIM_W = 300.0F;
	static const float GRAPH_DIM_H = 100.0F;

	float graph_x = GRAPH_POS_X;
	float graph_y = (float)app->height - GRAPH_POS_Y_OFF;
	float graph_w = GRAPH_DIM_W;
	float graph_h = GRAPH_DIM_H;
	float bar_w = graph_w / (float)size;

	/* Background */
	ui_draw_rect(&app->ui, graph_x, graph_y, graph_w, graph_h,
	             (vec3){0.0F, 0.0F, 0.0F}, app->width, app->height);

	/* Find peak for scaling */
	int max_bucket = 1;
	for (int i = 0; i < size; i++) {
		if (buckets[i] > max_bucket) {
			max_bucket = buckets[i];
		}
	}

	/* Draw Bars */
	for (int i = 0; i < size; i++) {
		float h_val = (float)buckets[i] / (float)max_bucket * graph_h;
		vec3 bar_col;
		glm_vec3_copy((float*)HISTO_BAR_COLOR_GREEN, bar_col);
		if (i < size / 2) {
			glm_vec3_copy((float*)HISTO_BAR_COLOR_BLUE, bar_col);
		} else {
			glm_vec3_copy((float*)HISTO_BAR_COLOR_RED, bar_col);
		}

		ui_draw_rect(&app->ui, graph_x + ((float)i * bar_w),
		             graph_y + (graph_h - h_val), bar_w, h_val, bar_col,
		             app->width, app->height);
	}

	/* Draw Range Info */
	char range_text[RANGE_TEXT_BUFFER_SIZE];
	(void)safe_snprintf(range_text, sizeof(range_text),
	                    "Log Lum Range: [%.2f, %.2f]", min_lum, max_lum);
	ui_draw_text(&app->ui, range_text, graph_x,
	             graph_y - GRAPH_TEXT_PADDING, (float*)GRAPH_TEXT_COLOR,
	             app->width, app->height);
}

void app_draw_debug_overlay(App* app)
{
	/* Auto Exposure Debug Text */
	if (postprocess_is_enabled(&app->postprocess, POSTFX_EXPOSURE_DEBUG)) {
		const GLStateBackup saved_state = render_utils_save_state();
		render_utils_setup_ui_state();

		draw_exposure_debug_text(app);

		/* -----------------------
		   Luminance Histogram
		   ----------------------- */
		const int HISTO_SIZE = 64;
		int buckets[HISTO_SIZE];
		float min_lum = 0.0F;
		float max_lum = 0.0F;

		if (compute_luminance_histogram(app, buckets, HISTO_SIZE,
		                                &min_lum, &max_lum) != 0) {
			draw_luminance_histogram_graph(app, buckets, HISTO_SIZE,
			                               min_lum, max_lum);
		}

		/* Cleanup */
		render_utils_restore_state(&saved_state);
	}
}

void app_render_ui(App* app)
{
	const GLStateBackup saved_state = render_utils_save_state();
	render_utils_setup_ui_state();

	/* --- Draw Main Info Overlay --- */
	UILayout layout;
	/* Start slightly offset from top-left */
	ui_layout_init(&layout, &app->ui, DEFAULT_FONT_OFFSET_X,
	               DEFAULT_FONT_OFFSET_Y, DEFAULT_SPACING, app->width,
	               app->height);

	/* Conditional text overlay rendering based on text_overlay_mode
	 */
	if (app->text_overlay_mode >= 1) {
		static const float MS_PER_SECOND = 1000.0F;
		char fps_text[MAX_FPS_TEXT_LENGTH];
		float current_fps = 0.0F;
		float frame_time_ms = 0.0F;

		if (app->fps_counter.average_frame_time > 0.0F) {
			current_fps =
			    1.0F / (float)app->fps_counter.average_frame_time;
			frame_time_ms =
			    (float)app->fps_counter.average_frame_time *
			    MS_PER_SECOND;
		}

		(void)safe_snprintf(fps_text, sizeof(fps_text),
		                    "FPS: %.1f (%.2f ms)", current_fps,
		                    frame_time_ms);

		ui_layout_text(&layout, fps_text, DEFAULT_FONT_COLOR);

		/* Adaptive Sampler Debug */
		if (app->text_overlay_mode >= 2) {
			static const size_t AVG_TEXT_SIZE = 64;
			char avg_text[AVG_TEXT_SIZE];
			float sampled_avg =
			    adaptive_sampler_get_average(&app->fps_sampler);

			/* Show numerical average */
			(void)safe_snprintf(avg_text, sizeof(avg_text),
			                    "Sampled Avg: %.2f", sampled_avg);
			ui_layout_text(&layout, avg_text, DEFAULT_FONT_COLOR);
		}
	}

	/* 2. Position - shown in modes 1, 2, 3 */
	if (app->text_overlay_mode >= 1) {
		char pos_text[DEBUG_TEXT_BUFFER_SIZE];
		(void)safe_snprintf(
		    pos_text, sizeof(pos_text), "Pos: %.1f, %.1f, %.1f",
		    app->camera.position[0], app->camera.position[1],
		    app->camera.position[2]);
		ui_layout_text(&layout, pos_text, DEFAULT_FONT_COLOR);
	}

	/* 3. Environment - shown in modes 2, 3 */
	if (app->text_overlay_mode >= 2 && app->hdr_count > 0 &&
	    app->current_hdr_index >= 0) {
		char env_text[ENV_TEXT_BUFFER_SIZE];
		(void)safe_snprintf(env_text, sizeof(env_text), "Env: %s",
		                    app->hdr_files[app->current_hdr_index]);
		ui_layout_text(&layout, env_text, ENV_TEXT_COLOR);
	}

	/* 4. Exposure - shown in mode 3 only */
	if (app->text_overlay_mode >= 3) {
		float exposure_val = 0.0F;
		if (postprocess_is_enabled(&app->postprocess,
		                           POSTFX_AUTO_EXPOSURE)) {
			glBindBuffer(GL_PIXEL_PACK_BUFFER, app->exposure_pbo);
			float* ptr = NULL;
			ptr = (float*)glMapBuffer(GL_PIXEL_PACK_BUFFER,
			                          GL_READ_ONLY);
			if (ptr) {
#ifdef TRACY_ENABLE
				TracyCZoneN(ctx, "AE PBO Map (Sync Wait)", 1);
#endif
				app->current_exposure = *ptr;
				glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
#ifdef TRACY_ENABLE
				TracyCZoneEnd(ctx);
#endif
			}
			glBindTexture(
			    GL_TEXTURE_2D,
			    app->postprocess.auto_exposure_fx.exposure_tex);
#ifdef TRACY_ENABLE
			TracyCZoneN(ctx2, "AE Fetch Trigger", 1);
#endif
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, 0);
#ifdef TRACY_ENABLE
			TracyCZoneEnd(ctx2);
#endif
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
			exposure_val = app->current_exposure;
		} else {
			exposure_val = app->postprocess.exposure.exposure;
		}

		char exposure_text[EXPOSURE_TEXT_BUFFER_SIZE];
		(void)safe_snprintf(exposure_text, sizeof(exposure_text),
		                    "Exposure: %.3f", exposure_val);
		ui_layout_text(&layout, exposure_text, ENV_TEXT_COLOR);
	}

	/* 5. IBL Processing Indicator */
	if (app->ibl_coord.state != IBL_STATE_IDLE || app->env_map_loading) {
		char loading_text[UI_LOADING_TEXT_SIZE];
		const char* status = (app->env_map_loading != 0)
		                         ? "Loading HDR"
		                         : "Generating IBL";
		(void)safe_snprintf(loading_text, sizeof(loading_text), "%s",
		                    status);

		float text_width =
		    (float)strlen(loading_text) * UI_LOADING_TEXT_WIDTH_FACTOR;
		float spinner_size = UI_SPINNER_SIZE;
		float center_x = (float)app->width * UI_CENTER_FACTOR;
		float center_y = (float)app->height * UI_CENTER_FACTOR;
		float text_x = center_x - (text_width * UI_CENTER_FACTOR);
		float text_y =
		    center_y + (spinner_size * UI_TEXT_OFFSET_FACTOR);

		ui_draw_text(&app->ui, loading_text, text_x, text_y,
		             (float*)HISTO_BAR_COLOR_BLUE, app->width,
		             app->height);

		double current_time = glfwGetTime();
		float angle = (float)current_time * (float)UI_SPINNER_SPEED;
		ui_draw_spinner(&app->ui, center_x, center_y, spinner_size,
		                angle, (float*)UI_SPINNER_COLOR, app->width,
		                app->height);
	}

	render_utils_restore_state(&saved_state);

	if (postprocess_is_enabled(&app->postprocess, POSTFX_EXPOSURE_DEBUG)) {
		app_draw_debug_overlay(app);
	}

	if (app->show_help) {
		app_draw_help_overlay(app);
	}

	gpu_profiler_ui_draw(&app->timeline_ui, &app->ui, app->width,
	                     app->height);

	action_notifier_draw(&app->notifier, &app->ui, app->width, app->height);
}
