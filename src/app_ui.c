#include "app_ui.h"

#include "action_notifier.h"
#include "adaptive_sampler.h"
#include "app.h"
#include "app_binding.h"
#include "app_settings.h"
#include "glad/glad.h"
#include "postprocess.h"
#include "texture.h"
#include "ui.h"
#include "utils.h"
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Forward Declarations (Internal) --- */
static void draw_exposure_overlay(const App* app, UILayout* layout);
static void draw_loading_indicator(const App* app);
static void draw_help_overlay_keys(const App* app, float start_x, float start_y,
                                   float total_h,
                                   const KeyboardLayoutConfig* scaled_cfg);

static void draw_key_layer_base(UIContext* ui_ctx, const AppUIOverlay* overlay,
                                const KeyboardLayoutConfig* scaled_cfg,
                                const KeyPos* pos, float pos_x, float pos_y,
                                const vec3 base_col, bool has_binding,
                                bool is_pressed, bool is_hovered,
                                float global_dim_mult, int screen_width,
                                int screen_height);

static void draw_key_layer_effects(UIContext* ui_ctx,
                                   const AppUIOverlay* overlay,
                                   const KeyboardLayoutConfig* scaled_cfg,
                                   const KeyPos* pos, float pos_x, float pos_y,
                                   const vec3 base_col, bool is_pressed,
                                   bool is_hovered, int screen_width,
                                   int screen_height);

static void draw_key_layer_label(UIContext* ui_ctx,
                                 const KeyboardLayoutConfig* scaled_cfg,
                                 const KeyPos* pos, float pos_x, float pos_y,
                                 int screen_width, int screen_height);

static void draw_text_centered(UIContext* ui_ctx, const char* text, float pos_x,
                               float pos_y, float scale, int screen_width,
                               int screen_height);

static const AppBinding* get_active_binding(const AppBindingRegistry* registry,
                                            int key, int mods,
                                            int* out_effective_mods);

void app_ui_init(AppUIOverlay* overlay)
{
	overlay->show_help = false;
	overlay->show_info_overlay = true;
	overlay->show_exposure_debug = false;
	overlay->text_overlay_mode = 0;
	overlay->help_hovered_key = -1;
	overlay->help_pressed_key = -1;
	overlay->help_pressed_mods = 0;
	overlay->help_press_timer = 0.0;
	overlay->help_global_dim = 1.0;
	overlay->help_hover_decay = 0.0;
	overlay->kbd_config.key_size = DEFAULT_KBD_KEY_SIZE;
	overlay->kbd_config.key_padding = DEFAULT_KBD_KEY_PADDING;
	overlay->kbd_config.key_radius = DEFAULT_KBD_KEY_RADIUS;
	overlay->kbd_config.label_scale = DEFAULT_KBD_LABEL_SCALE;
	overlay->kbd_config.title_y_offset = DEFAULT_KBD_TITLE_Y_OFFSET;
	overlay->kbd_config.detail_y_offset = DEFAULT_KBD_DETAIL_Y_OFFSET;
	ui_init(&overlay->ui, "assets/fonts/FiraCode-Regular.ttf",
	        DEFAULT_FONT_SIZE);

	/* Load cyberpunk keyboard PNG assets */
	overlay->kbd_tex_frame =
	    texture_load_rgba_png("assets/textures/ui/kbd_panel_frame.png");
	overlay->kbd_tex_key_base =
	    texture_load_rgba_png("assets/textures/ui/kbd_key_base.png");
	overlay->help_captured_camera = 0;
}

void app_ui_cleanup(AppUIOverlay* overlay)
{
	ui_destroy(&overlay->ui);
	if (overlay->kbd_tex_frame != 0U) {
		glDeleteTextures(1, &overlay->kbd_tex_frame);
		overlay->kbd_tex_frame = 0;
	}
	if (overlay->kbd_tex_key_base != 0U) {
		glDeleteTextures(1, &overlay->kbd_tex_key_base);
		overlay->kbd_tex_key_base = 0;
	}
}

void app_ui_update(AppUIOverlay* overlay, double delta_time)
{
	if (overlay->help_press_timer > 0.0) {
		overlay->help_press_timer -= delta_time;
		if (overlay->help_press_timer <= 0.0) {
			overlay->help_pressed_key = -1;
			overlay->help_pressed_mods = 0;
		}
	}

	/* Decay the hover timer */
	if (overlay->help_hover_decay > 0.0) {
		overlay->help_hover_decay -= delta_time;
	}

	/* Smooth global dimming: target is dimmed if a key is active, or was
	 * recently active (hover decay grace period), otherwise 1.0 */
	double target_dim = 1.0;
	if (overlay->help_pressed_key != -1 ||
	    overlay->help_hover_decay > 0.0 ||
	    overlay->help_hovered_key != -1) {
		target_dim = 1.0 - (double)GLOBAL_DIM_MAX_FALLOFF;
	}
	/* Interpolate current dim towards target dim (factor for quick but
	 * smooth transition) */
	overlay->help_global_dim += (target_dim - overlay->help_global_dim) *
	                            (double)GLOBAL_DIM_SMOOTH_FACTOR *
	                            delta_time;
}

void app_ui_handle_mouse(AppUIOverlay* overlay, double xpos, double ypos,
                         int screen_width, int screen_height)
{
	overlay->help_hovered_key = -1;

	if (!overlay->show_help) {
		return;
	}

	const unsigned int num_keys =
	    (unsigned int)(sizeof(KEY_LAYOUT_QWERTY) /
	                   sizeof(KEY_LAYOUT_QWERTY[0]));
	for (unsigned int i = 0; i < num_keys; i++) {
		const KeyPos* kpos = &KEY_LAYOUT_QWERTY[i];

		/* Center the keyboard layout (same logic as draw_help_overlay)
		 */
		const float ui_scale = glm_clamp(
		    (float)screen_height / DEFAULT_BASE_RESOLUTION_HEIGHT, 0.5F,
		    3.0F);
		const float scaled_key_size =
		    overlay->kbd_config.key_size * ui_scale;
		const float scaled_key_padding =
		    overlay->kbd_config.key_padding * ui_scale;

		const float total_w =
		    16.5F * (scaled_key_size + scaled_key_padding);
		const float total_h =
		    6.0F * (scaled_key_size + scaled_key_padding);
		const float start_x =
		    ((float)screen_width - total_w) * UI_CENTER_FACTOR;
		const float start_y =
		    ((float)screen_height - total_h) * UI_CENTER_FACTOR;

		const float kx_pos =
		    start_x +
		    (kpos->x_off * (scaled_key_size + scaled_key_padding));
		const float ky_pos =
		    start_y +
		    ((float)kpos->row * (scaled_key_size + scaled_key_padding));

		const float key_w = (kpos->width * scaled_key_size) +
		                    ((kpos->width - 1.0F) * scaled_key_padding);
		const float key_h = scaled_key_size;

		if (xpos >= (double)kx_pos &&
		    xpos <= (double)(kx_pos + key_w) &&
		    ypos >= (double)ky_pos &&
		    ypos <= (double)(ky_pos + key_h)) {
			overlay->help_hovered_key = kpos->key;
			overlay->help_hover_decay = HOVER_DECAY_DURATION;
			break;
		}
	}
}

static void draw_text_centered(UIContext* ui_ctx, const char* text, float pos_x,
                               float pos_y, float scale, int screen_width,
                               int screen_height)
{
	float text_w = ui_measure_text(ui_ctx, text) * scale;
	/* Logically const: drawing text to the batch is an UI side effect */
	ui_draw_text_scaled(
	    ui_ctx, text, pos_x - (text_w * UI_CENTER_FACTOR),
	    pos_y - ((DEFAULT_FONT_SIZE * scale) * UI_CENTER_FACTOR),
	    (float*)DEFAULT_FONT_COLOR, 1.0F, scale, screen_width,
	    screen_height);
}

static void draw_key_layer_base(UIContext* ui_ctx, const AppUIOverlay* overlay,
                                const KeyboardLayoutConfig* scaled_cfg,
                                const KeyPos* pos, float pos_x, float pos_y,
                                const vec3 base_col, bool has_binding,
                                bool is_pressed, bool is_hovered,
                                float global_dim_mult, int screen_width,
                                int screen_height)
{
	const float key_w = (pos->width * scaled_cfg->key_size) +
	                    ((pos->width - 1.0F) * scaled_cfg->key_padding);
	const float key_h = scaled_cfg->key_size;

	vec3 tint;
	glm_vec3_copy((float*)KEY_COLOR_DEFAULT, tint);
	float alpha = KEY_DEFAULT_ALPHA;

	if (has_binding) {
		glm_vec3_copy((float*)base_col, tint);
		alpha = DEFAULT_KBD_BOUND_ALPHA;
	}

	if (is_pressed || is_hovered) {
		glm_vec3_clamp(tint, KEY_PRESS_BRIGHTEN_MIN, 1.0F);
		alpha = KEY_PRESSED_ALPHA;
	}
	alpha *= global_dim_mult;

	if (overlay->kbd_tex_key_base != 0U) {
		ui_draw_textured_quad(ui_ctx, overlay->kbd_tex_key_base, pos_x,
		                      pos_y, key_w, key_h, tint, alpha,
		                      screen_width, screen_height);
	} else {
		ui_draw_rounded_rect(ui_ctx, pos_x, pos_y, key_w, key_h,
		                     scaled_cfg->key_radius, tint, alpha,
		                     screen_width, screen_height);
	}
}

static void draw_key_layer_effects(UIContext* ui_ctx,
                                   const AppUIOverlay* overlay,
                                   const KeyboardLayoutConfig* scaled_cfg,
                                   const KeyPos* pos, float pos_x, float pos_y,
                                   const vec3 base_col, bool is_pressed,
                                   bool is_hovered, int screen_width,
                                   int screen_height)
{
	const float key_w = (pos->width * scaled_cfg->key_size) +
	                    ((pos->width - 1.0F) * scaled_cfg->key_padding);
	const float key_h = scaled_cfg->key_size;

	if (is_pressed || is_hovered) {
		float bloom_alpha = GLOW_HOVER_ALPHA;
		if (is_pressed) {
			const float raw_intensity =
			    (float)(overlay->help_press_timer /
			            HELP_PRESS_DURATION);
			bloom_alpha =
			    glm_clamp(raw_intensity * BLOOM_PRESS_MAX_ALPHA,
			              0.0F, BLOOM_PRESS_MAX_ALPHA);
		}

		static const float GLOW_SIDES = 2.0F;
		const float scale_ratio =
		    scaled_cfg->key_size / overlay->kbd_config.key_size;
		const float glow_expand = 12.0F * scale_ratio;

		ui_draw_glow_rect(
		    ui_ctx, pos_x - glow_expand, pos_y - glow_expand,
		    key_w + (glow_expand * GLOW_SIDES),
		    key_h + (glow_expand * GLOW_SIDES),
		    scaled_cfg->key_radius + (glow_expand * UI_CENTER_FACTOR),
		    base_col, bloom_alpha, screen_width, screen_height);
	}
}

static void draw_key_layer_label(UIContext* ui_ctx,
                                 const KeyboardLayoutConfig* scaled_cfg,
                                 const KeyPos* pos, float pos_x, float pos_y,
                                 int screen_width, int screen_height)
{
	const float key_w = (pos->width * scaled_cfg->key_size) +
	                    ((pos->width - 1.0F) * scaled_cfg->key_padding);
	const float key_h = scaled_cfg->key_size;

	const float label_x = pos_x + (key_w * UI_CENTER_FACTOR);
	const float label_y = pos_y + (key_h * UI_CENTER_FACTOR);
	draw_text_centered(ui_ctx, pos->label, label_x, label_y,
	                   scaled_cfg->label_scale, screen_width,
	                   screen_height);
}

static void get_key_base_color(const AppBindingRegistry* registry, int key,
                               vec3 out_col, bool* out_has_binding)
{
	const AppBinding* direct = app_binding_registry_get(registry, key, 0);
	const AppBinding* shifted =
	    app_binding_registry_get(registry, key, (int)GLFW_MOD_SHIFT);
	const AppBinding* alted =
	    app_binding_registry_get(registry, key, (int)GLFW_MOD_ALT);

	glm_vec3_copy((float*)KEY_COLOR_DEFAULT, out_col);
	if (direct != NULL || shifted != NULL || alted != NULL) {
		*out_has_binding = true;
	} else {
		*out_has_binding = false;
	}

	/* Priority: if the key has BOTH a direct binding AND a Shift+ binding,
	 * show orange (Combination) to signal the dual-role.
	 * Otherwise use the type of the direct binding (Cycle=green,
	 * Toggle=cyan), or orange if there is only a shifted binding and no
	 * direct binding.
	 */
	if (direct != NULL && shifted != NULL) {
		/* Dual-role key: always show orange */
		glm_vec3_copy((float*)KEY_COLOR_COMBINATION, out_col);
	} else if (direct != NULL) {
		if (direct->type == BINDING_TYPE_CYCLE) {
			glm_vec3_copy((float*)KEY_COLOR_CYCLE, out_col);
		} else {
			glm_vec3_copy((float*)KEY_COLOR_TOGGLE, out_col);
		}
	} else if (shifted != NULL || alted != NULL) {
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

static const AppBinding* get_active_binding(const AppBindingRegistry* registry,
                                            int key, int mods,
                                            int* out_effective_mods)
{
	if (key == -1) {
		if (out_effective_mods != NULL) {
			*out_effective_mods = 0;
		}
		return NULL;
	}

	const AppBinding* binding =
	    app_binding_registry_get(registry, key, mods);
	if (binding != NULL) {
		if (out_effective_mods != NULL) {
			*out_effective_mods = mods;
		}
		return binding;
	}

	/* Fallback: try Shift */
	binding = app_binding_registry_get(registry, key, (int)GLFW_MOD_SHIFT);
	if (binding != NULL) {
		if (out_effective_mods != NULL) {
			*out_effective_mods = (int)GLFW_MOD_SHIFT;
		}
		return binding;
	}

	/* Fallback: try no mods */
	binding = app_binding_registry_get(registry, key, 0);
	if (binding != NULL) {
		if (out_effective_mods != NULL) {
			*out_effective_mods = 0;
		}
		return binding;
	}

	if (out_effective_mods != NULL) {
		*out_effective_mods = 0;
	}
	return NULL;
}

void app_draw_help_overlay(const App* app)
{
	if (!app->overlay.show_help) {
		return;
	}

	/* Center the keyboard layout */
	const float ui_scale = glm_clamp(
	    (float)app->height / DEFAULT_BASE_RESOLUTION_HEIGHT, 0.5F, 3.0F);

	KeyboardLayoutConfig scaled_cfg;
	scaled_cfg.key_size = app->overlay.kbd_config.key_size * ui_scale;
	scaled_cfg.key_padding = app->overlay.kbd_config.key_padding * ui_scale;
	scaled_cfg.key_radius = app->overlay.kbd_config.key_radius * ui_scale;
	scaled_cfg.label_scale = app->overlay.kbd_config.label_scale * ui_scale;
	scaled_cfg.title_y_offset =
	    app->overlay.kbd_config.title_y_offset * ui_scale;
	scaled_cfg.detail_y_offset =
	    app->overlay.kbd_config.detail_y_offset * ui_scale;

	const float total_w =
	    16.5F * (scaled_cfg.key_size + scaled_cfg.key_padding);
	const float total_h =
	    6.0F * (scaled_cfg.key_size + scaled_cfg.key_padding);

	const float start_x = ((float)app->width - total_w) * UI_CENTER_FACTOR;
	const float start_y = ((float)app->height - total_h) * UI_CENTER_FACTOR;

	/* Layer 1: Dark background behind everything */
	ui_draw_rect_ex((UIContext*)&app->overlay.ui, 0.0F, 0.0F,
	                (float)app->width, (float)app->height,
	                (float*)HELP_BG_COLOR, HELP_BG_ALPHA, app->width,
	                app->height);

	/* Layer 2: Cyberpunk panel frame PNG at partial opacity so the 3D scene
	 * peeks through the background. */
	if (app->overlay.kbd_tex_frame != 0U) {
		ui_draw_textured_quad(
		    (UIContext*)&app->overlay.ui, app->overlay.kbd_tex_frame,
		    0.0F, 0.0F, (float)app->width, (float)app->height,
		    (vec3){1.0F, 1.0F, 1.0F}, PANEL_FRAME_ALPHA, app->width,
		    app->height);
	}

	/* Exit Hint */
	ui_draw_text_scaled(
	    (UIContext*)&app->overlay.ui, "[ESC] TO EXIT HELP",
	    (float)app->width - (HELP_EXIT_HINT_X_OFF * ui_scale),
	    (float)app->height - (HELP_EXIT_HINT_Y_OFF * ui_scale),
	    (float*)KEY_COLOR_TOGGLE, HELP_TEXT_ALPHA, ui_scale, app->width,
	    app->height);

	/* Title — neon cyan cyberpunk style (single draw via ui_draw_text_ex)
	 */
	{
		const float title_txt_w =
		    ui_measure_text(&app->overlay.ui, "[ APPLICATION HELP ]") *
		    ui_scale;
		const float title_x = ((float)app->width * UI_CENTER_FACTOR) -
		                      (title_txt_w * UI_CENTER_FACTOR);
		const float title_y = start_y - scaled_cfg.title_y_offset;
		ui_draw_text_scaled((UIContext*)&app->overlay.ui,
		                    "[ APPLICATION HELP ]", title_x, title_y,
		                    (float*)CYBER_TITLE_COLOR, 1.0F, ui_scale,
		                    app->width, app->height);
	}

	/* Color Legend */
	const float legend_y =
	    start_y - (scaled_cfg.title_y_offset * LEGEND_Y_FACTOR);
	const float legend_x_start = (float)app->width * LEGEND_X_START_FACTOR;
	const float legend_step = (float)app->width * LEGEND_STEP_FACTOR;

	ui_draw_text_scaled((UIContext*)&app->overlay.ui, "■ Toggle (On/Off)",
	                    legend_x_start, legend_y, (float*)KEY_COLOR_TOGGLE,
	                    HELP_TEXT_ALPHA, ui_scale, app->width, app->height);
	ui_draw_text_scaled((UIContext*)&app->overlay.ui, "■ Cycle",
	                    legend_x_start + legend_step, legend_y,
	                    (float*)KEY_COLOR_CYCLE, HELP_TEXT_ALPHA, ui_scale,
	                    app->width, app->height);
	ui_draw_text_scaled(
	    (UIContext*)&app->overlay.ui, "■ Combination (Shift/Alt+)",
	    legend_x_start + (legend_step * LEGEND_COMBO_STEP_MULT), legend_y,
	    (float*)KEY_COLOR_COMBINATION, HELP_TEXT_ALPHA, ui_scale,
	    app->width, app->height);

	draw_help_overlay_keys(app, start_x, start_y, total_h, &scaled_cfg);
}

static bool is_key_active_in_overlay(int key_to_test, int active_key,
                                     int active_mods)
{
	if (active_key == key_to_test) {
		return true;
	}
	if (is_modifier_relevant(key_to_test, active_key, active_mods)) {
		return true;
	}
	return false;
}

static void draw_help_overlay_keys(const App* app, float start_x, float start_y,
                                   float total_h,
                                   const KeyboardLayoutConfig* scaled_cfg)
{
	const float ui_scale =
	    scaled_cfg->key_size / app->overlay.kbd_config.key_size;
	const float global_dim_mult = (float)app->overlay.help_global_dim;
	int effective_mods = 0;
	(void)get_active_binding(
	    &app->binding_registry, app->overlay.help_pressed_key,
	    app->overlay.help_pressed_mods, &effective_mods);

	int hovered_effective_mods = 0;
	if (app->overlay.help_hovered_key != -1) {
		(void)get_active_binding(
		    &app->binding_registry, app->overlay.help_hovered_key,
		    app->overlay.help_pressed_mods, &hovered_effective_mods);
	}

	const unsigned int num_keys =
	    (unsigned int)(sizeof(KEY_LAYOUT_QWERTY) /
	                   sizeof(KEY_LAYOUT_QWERTY[0]));

	/* Pass 1: Keycaps (Batching all textured quads using kbd_tex_key_base)
	 */
	for (unsigned int i = 0; i < num_keys; i++) {
		const KeyPos* kpos = &KEY_LAYOUT_QWERTY[i];
		const float kx_pos =
		    start_x + (kpos->x_off * (scaled_cfg->key_size +
		                              scaled_cfg->key_padding));
		const float ky_pos =
		    start_y + ((float)kpos->row * (scaled_cfg->key_size +
		                                   scaled_cfg->key_padding));

		vec3 base_col;
		bool has_binding = false;
		get_key_base_color(&app->binding_registry, kpos->key, base_col,
		                   &has_binding);

		bool is_pressed = is_key_active_in_overlay(
		    kpos->key, app->overlay.help_pressed_key, effective_mods);
		bool is_hovered = is_key_active_in_overlay(
		    kpos->key, app->overlay.help_hovered_key,
		    hovered_effective_mods);

		float current_key_dim =
		    (is_pressed || is_hovered) ? 1.0F : global_dim_mult;

		draw_key_layer_base((UIContext*)&app->overlay.ui, &app->overlay,
		                    scaled_cfg, kpos, kx_pos, ky_pos, base_col,
		                    has_binding, is_pressed, is_hovered,
		                    current_key_dim, app->width, app->height);
	}

	/* Pass 2: Bloom / Effects (Batching all procedural glow quads) */
	for (unsigned int i = 0; i < num_keys; i++) {
		const KeyPos* kpos = &KEY_LAYOUT_QWERTY[i];
		const float kx_pos =
		    start_x + (kpos->x_off * (scaled_cfg->key_size +
		                              scaled_cfg->key_padding));
		const float ky_pos =
		    start_y + ((float)kpos->row * (scaled_cfg->key_size +
		                                   scaled_cfg->key_padding));

		bool is_pressed = is_key_active_in_overlay(
		    kpos->key, app->overlay.help_pressed_key, effective_mods);
		bool is_hovered = is_key_active_in_overlay(
		    kpos->key, app->overlay.help_hovered_key,
		    hovered_effective_mods);

		if (is_pressed || is_hovered) {
			vec3 base_col;
			bool has_binding = false;
			get_key_base_color(&app->binding_registry, kpos->key,
			                   base_col, &has_binding);
			if (is_pressed || is_hovered) {
				glm_vec3_clamp(base_col, KEY_PRESS_BRIGHTEN_MIN,
				               1.0F);
			}

			draw_key_layer_effects(
			    (UIContext*)&app->overlay.ui, &app->overlay,
			    scaled_cfg, kpos, kx_pos, ky_pos, base_col,
			    is_pressed, is_hovered, app->width, app->height);
		}
	}

	/* Pass 3: Labels (Batching all font glyph quads) */
	for (unsigned int i = 0; i < num_keys; i++) {
		const KeyPos* kpos = &KEY_LAYOUT_QWERTY[i];
		const float kx_pos =
		    start_x + (kpos->x_off * (scaled_cfg->key_size +
		                              scaled_cfg->key_padding));
		const float ky_pos =
		    start_y + ((float)kpos->row * (scaled_cfg->key_size +
		                                   scaled_cfg->key_padding));

		draw_key_layer_label((UIContext*)&app->overlay.ui, scaled_cfg,
		                     kpos, kx_pos, ky_pos, app->width,
		                     app->height);
	}

	/* Show details for hovered or pressed key */
	const int target_key = (app->overlay.help_pressed_key != -1)
	                           ? app->overlay.help_pressed_key
	                           : app->overlay.help_hovered_key;
	if (target_key != -1) {
		const int desc_mods = (app->overlay.help_pressed_key != -1)
		                          ? app->overlay.help_pressed_mods
		                          : 0;
		const AppBinding* binding = get_active_binding(
		    &app->binding_registry, target_key, desc_mods, NULL);

		if (binding != NULL) {
			/* Show detailed description below help */
			const float detail_y =
			    start_y + total_h + scaled_cfg->detail_y_offset;
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
			draw_text_centered((UIContext*)&app->overlay.ui, buf,
			                   (float)app->width * UI_CENTER_FACTOR,
			                   detail_y, ui_scale, app->width,
			                   app->height);
		}
	}
}

static void draw_exposure_debug_text(const App* app)
{
	float exposure_val =
	    postprocess_get_exposure((PostProcess*)&app->postprocess);

	char debug_text[DEBUG_TEXT_BUFFER_SIZE];
	const float luminance =
	    (exposure_val > LUMINANCE_EPSILON) ? (1.0F / exposure_val) : 0.0F;
	(void)safe_snprintf(debug_text, sizeof(debug_text),
	                    "Auto Exposure: %.4f | Scene Lum: %.4f",
	                    exposure_val, luminance);

	ui_draw_text((UIContext*)&app->overlay.ui, debug_text,
	             DEFAULT_FONT_OFFSET_X,
	             DEFAULT_FONT_OFFSET_Y + DEBUG_TEXT_Y_OFFSET,
	             (float*)DEBUG_ORANGE_COLOR, app->width, app->height);
}

static void draw_luminance_histogram_graph(const App* app, const int* buckets,
                                           int size, float min_lum,
                                           float max_lum)
{
	const float graph_x = GRAPH_POS_X;
	const float graph_y = (float)app->height - GRAPH_POS_Y_OFF;
	const float graph_w = GRAPH_DIM_W;
	const float graph_h = GRAPH_DIM_H;
	const float bar_w = graph_w / (float)size;

	/* Background */
	ui_draw_rect((UIContext*)&app->overlay.ui, graph_x, graph_y, graph_w,
	             graph_h, (vec3){0.0F, 0.0F, 0.0F}, app->width,
	             app->height);

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

		ui_draw_rect((UIContext*)&app->overlay.ui, bx_pos, by_pos,
		             bar_w, bar_h, bar_col, app->width, app->height);
	}

	/* Labels for min/max */
	char range_text[RANGE_TEXT_BUFFER_SIZE];
	(void)safe_snprintf(range_text, sizeof(range_text), "%.2f .. %.2f",
	                    min_lum, max_lum);
	ui_draw_text((UIContext*)&app->overlay.ui, range_text, graph_x,
	             graph_y + graph_h + GRAPH_TEXT_PADDING, GRAPH_TEXT_COLOR,
	             app->width, app->height);
}

static void draw_bloom_debug_status(const App* app, UILayout* layout)
{
	if (postprocess_is_enabled((PostProcess*)&app->postprocess,
	                           POSTFX_BLOOM_DEBUG)) {
		char buf[DEBUG_TEXT_BUFFER_SIZE];
		int step = app->postprocess.bloom_fx.debug_step;
		int mip = app->postprocess.bloom_fx.debug_mip;
		const char* stages[] = {"Final Map", "Prefilter", "Downsample",
		                        "Upsample"};

		(void)safe_snprintf(buf, sizeof(buf),
		                    "Bloom Debug: %s | Mip: %d",
		                    stages[step % 4], mip);
		ui_layout_text(layout, buf, (float*)DEBUG_ORANGE_COLOR);
	}
}

void app_draw_debug_overlay(const App* app)
{
	int buckets[HISTO_BUCKETS];
	float min_lum = 0.0F;
	float max_lum = 0.0F;

	if (postprocess_compute_luminance_histogram(
	        (PostProcess*)&app->postprocess, app->frame_count, buckets,
	        HISTO_BUCKETS, &min_lum, &max_lum) > 0) {
		draw_luminance_histogram_graph(app, buckets, HISTO_BUCKETS,
		                               min_lum, max_lum);
		draw_exposure_debug_text(app);
	}
}

static void draw_main_info_overlay(const App* app, UILayout* layout)
{
	if (app->overlay.text_overlay_mode < 1) {
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

	if (app->overlay.text_overlay_mode >= 2) {
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
	if (app->overlay.text_overlay_mode >= 2 && app->scene.hdr_count > 0 &&
	    app->scene.current_hdr_index >= 0) {
		char env_text[ENV_TEXT_BUFFER_SIZE];
		(void)safe_snprintf(
		    env_text, sizeof(env_text), "Env: %s",
		    app->scene.hdr_files[app->scene.current_hdr_index]);
		ui_layout_text(layout, env_text, ENV_TEXT_COLOR);
	}
}

static void draw_exposure_overlay(const App* app, UILayout* layout)
{
	if (app->overlay.text_overlay_mode < 3) {
		return;
	}

	float exposure_val =
	    postprocess_get_exposure((PostProcess*)&app->postprocess);

	char exposure_text[EXPOSURE_TEXT_BUFFER_SIZE];
	(void)safe_snprintf(exposure_text, sizeof(exposure_text),
	                    "Exposure: %.3f", exposure_val);
	ui_layout_text(layout, exposure_text, ENV_TEXT_COLOR);
}

static void draw_loading_indicator(const App* app)
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

	ui_draw_text((UIContext*)&app->overlay.ui, loading_text, text_x, text_y,
	             (float*)HISTO_BAR_COLOR_BLUE, app->width, app->height);

	double current_time = glfwGetTime();
	float angle = (float)current_time * (float)UI_SPINNER_SPEED;
	ui_draw_spinner((UIContext*)&app->overlay.ui, center_x, center_y,
	                UI_SPINNER_SIZE, angle, (float*)UI_SPINNER_COLOR,
	                app->width, app->height);
}

void app_render_ui(const App* app)
{
	/* Logically const: drawing UI is viewed as a read-only op for the App
	 * state, even if it uses an internal batching buffer. */
	UIContext* ui_ctx = (UIContext*)&app->overlay.ui;

	/* Wrap everything in a single batch to minimize draw
	 * calls and state switches. Note: ui_begin saves state
	 * and ui_end restores it. */
	ui_begin(ui_ctx, app->width, app->height);

	UILayout layout;
	ui_layout_init(&layout, ui_ctx, DEFAULT_FONT_OFFSET_X,
	               DEFAULT_FONT_OFFSET_Y, DEFAULT_SPACING, app->width,
	               app->height);

	draw_main_info_overlay(app, &layout);
	draw_exposure_overlay(app, &layout);
	draw_bloom_debug_status(app, &layout);
	draw_loading_indicator(app);

	if (postprocess_is_enabled((PostProcess*)&app->postprocess,
	                           POSTFX_EXPOSURE_DEBUG)) {
		app_draw_debug_overlay(app);
	}

	if (app->overlay.show_help) {
		app_draw_help_overlay(app);
	}

	gpu_profiler_ui_draw((GPUProfilerUI*)&app->timeline_ui, ui_ctx,
	                     app->width, app->height);
	action_notifier_draw((ActionNotifier*)&app->notifier, ui_ctx,
	                     app->width, app->height);

	/* End global batch (restores state) */
	ui_end(ui_ctx);
}

int compute_luminance_histogram(const App* app, int* buckets, int size,
                                float* min_lum, float* max_lum)
{
	return postprocess_compute_luminance_histogram(
	    (PostProcess*)&app->postprocess, app->frame_count, buckets, size,
	    min_lum, max_lum);
}
