#include "app_ui.h"

#include "action_notifier.h"
#include "adaptive_sampler.h"
#include "app.h"
#include "app_binding.h"
#include "app_settings.h"
#include "glad/glad.h"
#include "postprocess.h"
#include "ui.h"
#include "utils.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Forward Declarations (Internal) --- */
static void draw_exposure_overlay(App* app, UILayout* layout);
static void draw_loading_indicator(App* app);
static void draw_help_overlay_keys(App* app, float start_x, float start_y,
                                   float total_h);

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

static const float KEY_HOVER_ALPHA = 0.7F;
static const vec3 KEY_COLOR_DEFAULT = {0.2F, 0.2F, 0.2F};
static const vec3 KEY_COLOR_TOGGLE = {0.0F, 0.8F, 0.9F}; /* Cyan: On/Off */
static const vec3 KEY_COLOR_CYCLE = {0.2F, 0.9F, 0.2F};  /* Green: Cycle */
static const vec3 KEY_COLOR_COMBINATION = {1.0F, 0.6F,
                                           0.1F}; /* Orange: Combo */
static const vec3 KEY_COLOR_PRESSED = {0.0F, 1.0F, 0.5F};
static const vec3 HELP_BG_COLOR = {0.05F, 0.05F, 0.07F};
static const float HELP_BG_ALPHA = 0.85F;
static const float KEY_PRESSED_ALPHA = 0.9F;
static const float KEY_DEFAULT_ALPHA = 0.4F;
enum {
	ROW_SYSTEM = 0,
	ROW_NUMBERS = 1,
	ROW_QWERTY = 2,
	ROW_ASDF = 3,
	ROW_ZXCV = 4,
	ROW_BOTTOM = 5,
	MODIFIER_BUFFER_SIZE = 16,
	KEYBOARD_BUFFER_SIZE = 256
};

typedef struct {
	int key;
	int row;
	float x_off; /* In units of KEY_SIZE + KEY_PADDING */
	float width; /* In units of KEY_SIZE */
	const char* label;
} KeyPos;

static const KeyPos KEY_LAYOUT_QWERTY[] = {
    /* Row 0: Esc + Func */
    {GLFW_KEY_ESCAPE, ROW_SYSTEM, 0.0F, 1.0F, "Esc"},
    {GLFW_KEY_F1, ROW_SYSTEM, 2.0F, 1.0F, "F1"},
    {GLFW_KEY_F2, ROW_SYSTEM, 3.0F, 1.0F, "F2"},
    {GLFW_KEY_F3, ROW_SYSTEM, 4.0F, 1.0F, "F3"},
    {GLFW_KEY_F4, ROW_SYSTEM, 5.0F, 1.0F, "F4"},
    {GLFW_KEY_F5, ROW_SYSTEM, 6.5F, 1.0F, "F5"},
    {GLFW_KEY_F9, ROW_SYSTEM, 11.5F, 1.0F, "F9"},
    {GLFW_KEY_F12, ROW_SYSTEM, 14.5F, 1.2F, "F12"},

    /* Row 1: Numbers/Symbols */
    {GLFW_KEY_GRAVE_ACCENT, ROW_NUMBERS, 0.0F, 1.0F, "~"},
    {GLFW_KEY_1, ROW_NUMBERS, 1.0F, 1.0F, "1"},
    {GLFW_KEY_2, ROW_NUMBERS, 2.0F, 1.0F, "2"},
    {GLFW_KEY_3, ROW_NUMBERS, 3.0F, 1.0F, "3"},
    {GLFW_KEY_4, ROW_NUMBERS, 4.0F, 1.0F, "4"},
    {GLFW_KEY_5, ROW_NUMBERS, 5.0F, 1.0F, "5"},
    {GLFW_KEY_6, ROW_NUMBERS, 6.0F, 1.0F, "6"},
    {GLFW_KEY_7, ROW_NUMBERS, 7.0F, 1.0F, "7"},
    {GLFW_KEY_8, ROW_NUMBERS, 8.0F, 1.0F, "8"},
    {GLFW_KEY_9, ROW_NUMBERS, 9.0F, 1.0F, "9"},
    {GLFW_KEY_0, ROW_NUMBERS, 10.0F, 1.0F, "0"},

    /* Row 2: QWERTY */
    {GLFW_KEY_TAB, ROW_QWERTY, 0.0F, 1.5F, "Tab"},
    {GLFW_KEY_Q, ROW_QWERTY, 1.5F, 1.0F, "Q"},
    {GLFW_KEY_W, ROW_QWERTY, 2.5F, 1.0F, "W"},
    {GLFW_KEY_E, ROW_QWERTY, 3.5F, 1.0F, "E"},
    {GLFW_KEY_R, ROW_QWERTY, 4.5F, 1.0F, "R"},
    {GLFW_KEY_T, ROW_QWERTY, 5.5F, 1.0F, "T"},
    {GLFW_KEY_Y, ROW_QWERTY, 6.5F, 1.0F, "Y"},
    {GLFW_KEY_U, ROW_QWERTY, 7.5F, 1.0F, "U"},
    {GLFW_KEY_I, ROW_QWERTY, 8.5F, 1.0F, "I"},
    {GLFW_KEY_O, ROW_QWERTY, 9.5F, 1.0F, "O"},
    {GLFW_KEY_P, ROW_QWERTY, 10.5F, 1.0F, "P"},

    /* Row 3: ASDF */
    {GLFW_KEY_CAPS_LOCK, ROW_ASDF, 0.0F, 1.8F, "Caps"},
    {GLFW_KEY_A, ROW_ASDF, 1.8F, 1.0F, "A"},
    {GLFW_KEY_S, ROW_ASDF, 2.8F, 1.0F, "S"},
    {GLFW_KEY_D, ROW_ASDF, 3.8F, 1.0F, "D"},
    {GLFW_KEY_F, ROW_ASDF, 4.8F, 1.0F, "F"},
    {GLFW_KEY_G, ROW_ASDF, 5.8F, 1.0F, "G"},
    {GLFW_KEY_H, ROW_ASDF, 6.8F, 1.0F, "H"},
    {GLFW_KEY_J, ROW_ASDF, 7.8F, 1.0F, "J"},
    {GLFW_KEY_K, ROW_ASDF, 8.8F, 1.0F, "K"},
    {GLFW_KEY_L, ROW_ASDF, 9.8F, 1.0F, "L"},

    /* Row 4: ZXCV */
    {GLFW_KEY_LEFT_SHIFT, ROW_ZXCV, 0.0F, 2.3F, "Shift"},
    {GLFW_KEY_Z, ROW_ZXCV, 2.3F, 1.0F, "Z"},
    {GLFW_KEY_X, ROW_ZXCV, 3.3F, 1.0F, "X"},
    {GLFW_KEY_C, ROW_ZXCV, 4.3F, 1.0F, "C"},
    {GLFW_KEY_V, ROW_ZXCV, 5.3F, 1.0F, "V"},
    {GLFW_KEY_B, ROW_ZXCV, 6.3F, 1.0F, "B"},
    {GLFW_KEY_N, ROW_ZXCV, 7.3F, 1.0F, "N"},
    {GLFW_KEY_M, ROW_ZXCV, 8.3F, 1.0F, "M"},

    /* Row 5: Space/System */
    {GLFW_KEY_LEFT_CONTROL, ROW_BOTTOM, 0.0F, 1.5F, "Ctrl"},
    {GLFW_KEY_SPACE, ROW_BOTTOM, 3.8F, 6.0F, "Space"},
    {GLFW_KEY_PAGE_UP, ROW_ZXCV, 13.5F, 1.3F, "PgUp"},
    {GLFW_KEY_PAGE_DOWN, ROW_BOTTOM, 13.5F, 1.3F, "PgDn"},
    {GLFW_KEY_UP, ROW_ZXCV, 15.0F, 1.0F, "Up"},
    {GLFW_KEY_DOWN, ROW_BOTTOM, 15.0F, 1.0F, "Dn"}};

static void draw_text_centered(App* app, const char* text, float pos_x,
                               float pos_y)
{
	float text_w = ui_measure_text(&app->ui, text);
	ui_draw_text(&app->ui, text, pos_x - (text_w * UI_CENTER_FACTOR),
	             pos_y - (DEFAULT_FONT_SIZE * UI_CENTER_FACTOR),
	             (float*)DEFAULT_FONT_COLOR, app->width, app->height);
}

static void draw_key(App* app, const KeyPos* pos, float pos_x, float pos_y,
                     const vec3 base_col, bool has_binding, bool is_pressed)
{
	float key_w = (pos->width * app->kbd_config.key_size) +
	              ((pos->width - 1.0F) * app->kbd_config.key_padding);
	float key_h = app->kbd_config.key_size;

	vec3 col;
	glm_vec3_copy((float*)KEY_COLOR_DEFAULT, col);
	float alpha = KEY_DEFAULT_ALPHA;

	if (is_pressed) {
		glm_vec3_copy((float*)KEY_COLOR_PRESSED, col);
		alpha = KEY_PRESSED_ALPHA;
	} else if (has_binding) {
		glm_vec3_copy((float*)base_col, col);
		alpha = KEY_HOVER_ALPHA;
	}

	ui_draw_rounded_rect(&app->ui, pos_x, pos_y, key_w, key_h,
	                     app->kbd_config.key_radius, col, alpha, app->width,
	                     app->height);

	/* Label */
	float label_x = pos_x + (key_w * UI_CENTER_FACTOR);
	float label_y = pos_y + (key_h * UI_CENTER_FACTOR);
	draw_text_centered(app, pos->label, label_x, label_y);

	/* Mouse Hover Logic */
	double mouse_x = 0.0;
	double mouse_y = 0.0;
	glfwGetCursorPos(app->window, &mouse_x, &mouse_y);
	if (mouse_x >= (double)pos_x && mouse_x <= (double)(pos_x + key_w) &&
	    mouse_y >= (double)pos_y && mouse_y <= (double)(pos_y + key_h)) {
		app->help_hovered_key = pos->key;
	}
}

/* UI Layout Constants */
static const float HELP_EXIT_HINT_X_OFF = 300.0F;
static const float HELP_EXIT_HINT_Y_OFF = 50.0F;
static const float HELP_TEXT_ALPHA = 0.8F;

static const float LEGEND_Y_FACTOR = 0.5F;
static const float LEGEND_X_START_FACTOR = 0.3F;
static const float LEGEND_STEP_FACTOR = 0.15F;
static const float LEGEND_COMBO_STEP_MULT = 2.0F;

static void get_key_base_color(const AppBindingRegistry* registry, int key,
                               vec3 out_col, bool* out_has_binding)
{
	const AppBinding* direct = app_binding_registry_get(registry, key, 0);
	const AppBinding* shifted =
	    app_binding_registry_get(registry, key, (int)GLFW_MOD_SHIFT);

	glm_vec3_copy((float*)KEY_COLOR_DEFAULT, out_col);
	if (direct != NULL || shifted != NULL) {
		*out_has_binding = true;
	} else {
		*out_has_binding = false;
	}

	if (direct != NULL) {
		if (direct->type == BINDING_TYPE_CYCLE) {
			glm_vec3_copy((float*)KEY_COLOR_CYCLE, out_col);
		} else {
			glm_vec3_copy((float*)KEY_COLOR_TOGGLE, out_col);
		}
	} else if (shifted != NULL) {
		glm_vec3_copy((float*)KEY_COLOR_COMBINATION, out_col);
	}
}

static bool is_modifier_relevant(int key, int pressed_key, int pressed_mods)
{
	if (pressed_key == -1) {
		return false;
	}
	if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) {
		return ((unsigned int)pressed_mods &
		        (unsigned int)GLFW_MOD_SHIFT) != 0U;
	}
	if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) {
		return ((unsigned int)pressed_mods &
		        (unsigned int)GLFW_MOD_CONTROL) != 0U;
	}
	if (key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT) {
		return ((unsigned int)pressed_mods &
		        (unsigned int)GLFW_MOD_ALT) != 0U;
	}
	return false;
}

void app_draw_help_overlay(App* app)
{
	if (!app->show_help) {
		return;
	}

	/* Center the keyboard layout */
	const float total_w =
	    16.5F * (app->kbd_config.key_size + app->kbd_config.key_padding);
	const float total_h =
	    6.0F * (app->kbd_config.key_size + app->kbd_config.key_padding);

	const float start_x = ((float)app->width - total_w) * UI_CENTER_FACTOR;
	const float start_y = ((float)app->height - total_h) * UI_CENTER_FACTOR;

	/* Background Overlay */
	ui_draw_rect_ex(&app->ui, 0.0F, 0.0F, (float)app->width,
	                (float)app->height, (float*)HELP_BG_COLOR,
	                HELP_BG_ALPHA, app->width, app->height);

	/* Exit Hint */
	ui_draw_text_ex(&app->ui, "[ESC] TO EXIT HELP",
	                (float)app->width - HELP_EXIT_HINT_X_OFF,
	                (float)app->height - HELP_EXIT_HINT_Y_OFF,
	                (float*)KEY_COLOR_TOGGLE, HELP_TEXT_ALPHA, app->width,
	                app->height);

	/* Title */
	draw_text_centered(app, "--- APPLICATION HELP (Dry-Run Mode) ---",
	                   (float)app->width * UI_CENTER_FACTOR,
	                   start_y - app->kbd_config.title_y_offset);

	/* Color Legend */
	const float legend_y =
	    start_y - (app->kbd_config.title_y_offset * LEGEND_Y_FACTOR);
	const float legend_x_start = (float)app->width * LEGEND_X_START_FACTOR;
	const float legend_step = (float)app->width * LEGEND_STEP_FACTOR;

	ui_draw_text_ex(&app->ui, "Toggle (On/Off)", legend_x_start, legend_y,
	                (float*)KEY_COLOR_TOGGLE, HELP_TEXT_ALPHA, app->width,
	                app->height);
	ui_draw_text_ex(&app->ui, "Cycle", legend_x_start + legend_step,
	                legend_y, (float*)KEY_COLOR_CYCLE, HELP_TEXT_ALPHA,
	                app->width, app->height);
	ui_draw_text_ex(&app->ui, "Combination (Shift+)",
	                legend_x_start + (legend_step * LEGEND_COMBO_STEP_MULT),
	                legend_y, (float*)KEY_COLOR_COMBINATION,
	                HELP_TEXT_ALPHA, app->width, app->height);

	draw_help_overlay_keys(app, start_x, start_y, total_h);
}

static void draw_help_overlay_keys(App* app, float start_x, float start_y,
                                   float total_h)
{
	app->help_hovered_key = -1;

	const unsigned int num_keys =
	    (unsigned int)(sizeof(KEY_LAYOUT_QWERTY) /
	                   sizeof(KEY_LAYOUT_QWERTY[0]));
	for (unsigned int i = 0; i < num_keys; i++) {
		const KeyPos* kpos = &KEY_LAYOUT_QWERTY[i];
		const float kx_pos =
		    start_x + (kpos->x_off * (app->kbd_config.key_size +
		                              app->kbd_config.key_padding));
		const float ky_pos =
		    start_y +
		    ((float)kpos->row *
		     (app->kbd_config.key_size + app->kbd_config.key_padding));

		/* Hit test for mouse interaction & color coding */
		vec3 base_col;
		bool has_binding = false;
		get_key_base_color(&app->binding_registry, kpos->key, base_col,
		                   &has_binding);

		bool is_pressed = (app->help_pressed_key == kpos->key);

		/* Also highlight modifiers if they are part of a combination */
		if (!is_pressed &&
		    is_modifier_relevant(kpos->key, app->help_pressed_key,
		                         app->help_pressed_mods)) {
			is_pressed = true;
		}

		draw_key(app, kpos, kx_pos, ky_pos, base_col, has_binding,
		         is_pressed);
	}

	/* Show details for hovered or pressed key */
	const int target_key = (app->help_pressed_key != -1)
	                           ? app->help_pressed_key
	                           : app->help_hovered_key;
	if (target_key != -1) {
		const AppBinding* binding = NULL;
		if (app->help_pressed_key != -1) {
			binding = app_binding_registry_get(
			    &app->binding_registry, target_key,
			    app->help_pressed_mods);
		}

		if (binding == NULL) {
			binding = app_binding_registry_get(
			    &app->binding_registry, target_key,
			    (int)GLFW_MOD_SHIFT);
		}
		if (binding == NULL) {
			binding = app_binding_registry_get(
			    &app->binding_registry, target_key, 0);
		}

		if (binding != NULL) {
			/* Show detailed description below help */
			const float detail_y =
			    start_y + total_h + app->kbd_config.detail_y_offset;
			char buf[KEYBOARD_BUFFER_SIZE];
			char mod_str[MODIFIER_BUFFER_SIZE] = "";

			if (((unsigned int)binding->mods &
			     (unsigned int)GLFW_MOD_SHIFT) != 0U) {
				(void)strcpy(mod_str, "SHIFT+");
			} else if (((unsigned int)binding->mods &
			            (unsigned int)GLFW_MOD_CONTROL) != 0U) {
				(void)strcpy(mod_str, "CTRL+");
			} else if (((unsigned int)binding->mods &
			            (unsigned int)GLFW_MOD_ALT) != 0U) {
				(void)strcpy(mod_str, "ALT+");
			}

			(void)safe_snprintf(buf, sizeof(buf), "[%s%s]: %s",
			                    mod_str, binding->action,
			                    binding->desc);
			draw_text_centered(app, buf,
			                   (float)app->width * UI_CENTER_FACTOR,
			                   detail_y);
		}
	}
}

static void draw_exposure_debug_text(App* app)
{
	const float exposure_val = postprocess_get_exposure(&app->postprocess);

	char debug_text[DEBUG_TEXT_BUFFER_SIZE];
	const float luminance =
	    (exposure_val > LUMINANCE_EPSILON) ? (1.0F / exposure_val) : 0.0F;
	(void)safe_snprintf(debug_text, sizeof(debug_text),
	                    "Auto Exposure: %.4f | Scene Lum: %.4f",
	                    exposure_val, luminance);

	ui_draw_text(&app->ui, debug_text, DEFAULT_FONT_OFFSET_X,
	             DEFAULT_FONT_OFFSET_Y + DEBUG_TEXT_Y_OFFSET,
	             (float*)DEBUG_ORANGE_COLOR, app->width, app->height);
}

static void draw_luminance_histogram_graph(App* app, const int* buckets,
                                           int size, float min_lum,
                                           float max_lum)
{
	static const float GRAPH_POS_X = 20.0F;
	static const float GRAPH_POS_Y_OFF = 200.0F;
	static const float GRAPH_DIM_W = 300.0F;
	static const float GRAPH_DIM_H = 100.0F;

	const float graph_x = GRAPH_POS_X;
	const float graph_y = (float)app->height - GRAPH_POS_Y_OFF;
	const float graph_w = GRAPH_DIM_W;
	const float graph_h = GRAPH_DIM_H;
	const float bar_w = graph_w / (float)size;

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

	/* Draw bars */
	for (int i = 0; i < size; i++) {
		const float bar_h =
		    ((float)buckets[i] / (float)max_bucket) * graph_h;
		const float bx_pos = graph_x + ((float)i * bar_w);
		const float by_pos = graph_y + graph_h - bar_h;

		vec3 bar_col;
		glm_vec3_copy((float*)HISTO_BAR_COLOR_GREEN, bar_col);

		/* Color coding for specific areas */
		if (i < size / 4) {
			glm_vec3_copy((float*)HISTO_BAR_COLOR_BLUE, bar_col);
		} else if (i > (3 * size) / 4) {
			glm_vec3_copy((float*)HISTO_BAR_COLOR_RED, bar_col);
		}

		ui_draw_rect(&app->ui, bx_pos, by_pos, bar_w, bar_h, bar_col,
		             app->width, app->height);
	}

	/* Labels for min/max */
	char range_text[RANGE_TEXT_BUFFER_SIZE];
	(void)safe_snprintf(range_text, sizeof(range_text), "%.2f .. %.2f",
	                    min_lum, max_lum);
	ui_draw_text(&app->ui, range_text, graph_x,
	             graph_y + graph_h + GRAPH_TEXT_PADDING, GRAPH_TEXT_COLOR,
	             app->width, app->height);
}

void app_draw_debug_overlay(App* app)
{
	static const int HISTO_BUCKETS = 256;
	int buckets[HISTO_BUCKETS];
	float min_lum = 0.0F;
	float max_lum = 0.0F;

	if (postprocess_compute_luminance_histogram(
	        &app->postprocess, app->frame_count, buckets, HISTO_BUCKETS,
	        &min_lum, &max_lum) > 0) {
		draw_luminance_histogram_graph(app, buckets, HISTO_BUCKETS,
		                               min_lum, max_lum);
		draw_exposure_debug_text(app);
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
	/* Wrap everything in a single batch to minimize draw
	 * calls and state switches. Note: ui_begin saves state
	 * and ui_end restores it. */
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
