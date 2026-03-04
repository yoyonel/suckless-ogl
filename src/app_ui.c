#include "app_ui.h"

#include "action_notifier.h"
#include "adaptive_sampler.h"
#include "app.h"
#include "app_settings.h"
#include "glad/glad.h"
#include "postprocess.h"
#include "ui.h"
#include "utils.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Forward Declarations (Internal) --- */
static void draw_main_info_overlay(App* app, UILayout* layout);
static void draw_exposure_overlay(App* app, UILayout* layout);
static void draw_loading_indicator(App* app);

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

	/* Section: Features --- */
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
	ui_layout_text(&layout, "[Y] Toggle GI 1-Bounce", HELP_COLOR);
	ui_layout_text(&layout, "[Shift+Y] Toggle Probe Debug", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: Environment --- */
	ui_layout_text(&layout, "--- Environment ---", HELP_COLOR);
	ui_layout_text(&layout, "[PgUp/PgDn] Change HDR", HELP_COLOR);
	ui_layout_text(&layout, "[Shift + PgUp/PgDn] Blur HDR", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: Post-Process Styles --- */
	ui_layout_text(&layout, "--- Styles (Numpad) ---", HELP_COLOR);
	ui_layout_text(&layout, "[1] Default (Clean)", HELP_COLOR);
	ui_layout_text(&layout, "[2] Subtle", HELP_COLOR);
	ui_layout_text(&layout, "[3] Cinematic", HELP_COLOR);
	ui_layout_text(&layout, "[4] Vintage", HELP_COLOR);
	ui_layout_text(&layout, "[5] Matrix", HELP_COLOR);
	ui_layout_text(&layout, "[6] BW Contrast", HELP_COLOR);
	ui_layout_text(&layout, "[0] Reset", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: System --- */
	ui_layout_text(&layout, "--- System ---", HELP_COLOR);
	ui_layout_text(&layout, "[F9] Toggle Performance Mode", HELP_COLOR);
	ui_layout_text(&layout, "[F12] Take Screenshot", HELP_COLOR);
}

static void draw_exposure_debug_text(App* app)
{
	float exposure_val = postprocess_get_exposure(&app->postprocess);

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

/* All histogram logic moved to PostProcess */

static void draw_luminance_histogram_graph(App* app, const int* buckets,
                                           int size, float min_lum,
                                           float max_lum)
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
		draw_exposure_debug_text(app);

		/* -----------------------
		   Luminance Histogram
		   ----------------------- */
		const int HISTO_SIZE = 64;
		int buckets[HISTO_SIZE];
		float min_lum = 0.0F;
		float max_lum = 0.0F;

		if (postprocess_compute_luminance_histogram(
		        &app->postprocess, app->frame_count, buckets,
		        HISTO_SIZE, &min_lum, &max_lum) != 0) {
			draw_luminance_histogram_graph(app, buckets, HISTO_SIZE,
			                               min_lum, max_lum);
		}
	}
}

static void draw_main_info_overlay(App* app, UILayout* layout)
{
	if (app->text_overlay_mode < 1) {
		return;
	}

	/* 1. FPS & Sampler */
	static const float MS_PER_SECOND = 1000.0F;
	char fps_text[MAX_FPS_TEXT_LENGTH];
	float current_fps = 0.0F;
	float frame_time_ms = 0.0F;

	if (app->fps_counter.average_frame_time > 0.0F) {
		current_fps = 1.0F / (float)app->fps_counter.average_frame_time;
		frame_time_ms =
		    (float)app->fps_counter.average_frame_time * MS_PER_SECOND;
	}

	(void)safe_snprintf(fps_text, sizeof(fps_text), "FPS: %.1f (%.2f ms)",
	                    current_fps, frame_time_ms);
	ui_layout_text(layout, fps_text, DEFAULT_FONT_COLOR);

	if (app->text_overlay_mode >= 2) {
		static const size_t AVG_TEXT_SIZE = 64;
		char avg_text[AVG_TEXT_SIZE];
		float sampled_avg =
		    adaptive_sampler_get_average(&app->fps_sampler);
		(void)safe_snprintf(avg_text, sizeof(avg_text),
		                    "Sampled Avg: %.2f", sampled_avg);
		ui_layout_text(layout, avg_text, DEFAULT_FONT_COLOR);
	}

	/* 2. Position */
	char pos_text[DEBUG_TEXT_BUFFER_SIZE];
	(void)safe_snprintf(pos_text, sizeof(pos_text), "Pos: %.1f, %.1f, %.1f",
	                    app->camera.position[0], app->camera.position[1],
	                    app->camera.position[2]);
	ui_layout_text(layout, pos_text, DEFAULT_FONT_COLOR);

	/* 3. Environment */
	if (app->text_overlay_mode >= 2 && app->scene.hdr_count > 0 &&
	    app->scene.current_hdr_index >= 0) {
		char env_text[ENV_TEXT_BUFFER_SIZE];
		(void)safe_snprintf(
		    env_text, sizeof(env_text), "Env: %s",
		    app->scene.hdr_files[app->scene.current_hdr_index]);
		ui_layout_text(layout, env_text, ENV_TEXT_COLOR);
	}
}

/* Readbacks now handled in PostProcess */

static void draw_exposure_overlay(App* app, UILayout* layout)
{
	if (app->text_overlay_mode < 3) {
		return;
	}

	float exposure_val = postprocess_get_exposure(&app->postprocess);

	char exposure_text[EXPOSURE_TEXT_BUFFER_SIZE];
	(void)safe_snprintf(exposure_text, sizeof(exposure_text),
	                    "Exposure: %.3f", exposure_val);
	ui_layout_text(layout, exposure_text, ENV_TEXT_COLOR);
}

static void draw_loading_indicator(App* app)
{
	if (app->scene.ibl_coord.state == IBL_STATE_IDLE &&
	    !app->env_mgr.env_map_loading) {
		return;
	}

	char loading_text[UI_LOADING_TEXT_SIZE];
	const char* status = (app->env_mgr.env_map_loading != 0)
	                         ? "Loading HDR"
	                         : "Generating IBL";
	(void)safe_snprintf(loading_text, sizeof(loading_text), "%s", status);

	float text_width =
	    (float)strlen(loading_text) * UI_LOADING_TEXT_WIDTH_FACTOR;
	float center_x = (float)app->width * UI_CENTER_FACTOR;
	float center_y = (float)app->height * UI_CENTER_FACTOR;
	float text_x = center_x - (text_width * UI_CENTER_FACTOR);
	float text_y = center_y + (UI_SPINNER_SIZE * UI_TEXT_OFFSET_FACTOR);

	ui_draw_text(&app->ui, loading_text, text_x, text_y,
	             (float*)HISTO_BAR_COLOR_BLUE, app->width, app->height);

	double current_time = glfwGetTime();
	float angle = (float)current_time * (float)UI_SPINNER_SPEED;
	ui_draw_spinner(&app->ui, center_x, center_y, UI_SPINNER_SIZE, angle,
	                (float*)UI_SPINNER_COLOR, app->width, app->height);
}

void app_render_ui(App* app)
{
	/* Wrap everything in a single batch to minimize draw calls and state
	 * switches. Note: ui_begin saves state and ui_end restores it. */
	ui_begin(&app->ui, app->width, app->height);

	UILayout layout;
	ui_layout_init(&layout, &app->ui, DEFAULT_FONT_OFFSET_X,
	               DEFAULT_FONT_OFFSET_Y, DEFAULT_SPACING, app->width,
	               app->height);

	draw_main_info_overlay(app, &layout);
	draw_exposure_overlay(app, &layout);
	draw_loading_indicator(app);

	if (postprocess_is_enabled(&app->postprocess, POSTFX_EXPOSURE_DEBUG)) {
		app_draw_debug_overlay(app);
	}

	if (app->show_help) {
		app_draw_help_overlay(app);
	}

	gpu_profiler_ui_draw(&app->timeline_ui, &app->ui, app->width,
	                     app->height);
	action_notifier_draw(&app->notifier, &app->ui, app->width, app->height);

	/* End global batch (restores state) */
	ui_end(&app->ui);
}
