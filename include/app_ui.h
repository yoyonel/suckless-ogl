/**
 * @file app_ui.h
 * @brief UI rendering module for the application.
 *
 * This module handles all UI-related drawing operations, including help
 * overlays, debug information, and luminance statistics.
 */

#ifndef APP_UI_H
#define APP_UI_H
#include "glad/glad.h"
#include "ui.h"
#include <stdbool.h>

/* Histogram bucket count (used by tests and postprocess) */
enum { HISTO_BUCKETS = 256 };

/* Highlight duration for key-press animation (shared with app_input.c) */
static const float HELP_PRESS_DURATION = 2.0F;

/**
 * @enum HelpMode
 * @brief States for the F2 help overlay cycling.
 */
typedef enum {
	HELP_MODE_OFF = 0,
	HELP_MODE_KEYBOARD,
	HELP_MODE_GAMEPAD,
	HELP_MODE_COUNT
} HelpMode;

typedef struct {
	float key_size;
	float key_padding;
	float key_radius;
	float label_scale;
	float title_y_offset;
	float detail_y_offset;
} KeyboardLayoutConfig;

typedef struct AppUIOverlay {
	UIContext ui;
	KeyboardLayoutConfig kbd_config;

	HelpMode show_help;
	bool show_info_overlay;
	int text_overlay_mode;
	bool show_exposure_debug;

	int help_hovered_key;
	int help_pressed_key;
	int help_pressed_mods;
	double help_press_timer;
	double help_global_dim;
	double help_hover_decay; /**< Grace period for dimming when mouse leaves
	                          * a key
	                          */
	double help_gp_decay; /**< Grace period for dimming when gamepad input
	                       * stops — same role as help_hover_decay but for
	                       * gamepad controls.
	                       */

	/* Cyberpunk keyboard overlay textures (PNG assets) */
	GLuint
	    kbd_tex_frame; /**< Panel frame texture (cyan border + scanlines) */
	GLuint
	    kbd_tex_key_base; /**< Single keycap texture (tinted per binding) */

	/** Set to 1 when the overlay auto-disabled the camera on open, so we
	 *  can re-enable it exactly the same way (simulate 'C' press) on close.
	 */
	bool help_captured_camera;
} AppUIOverlay;

typedef struct App App;

/**
 * @brief Draws the help overlay with keyboard shortcuts.
 * @param app Pointer to the application state.
 */
void app_draw_help_overlay(const App* app);

/**
 * @brief Draws the gamepad help overlay with controller bindings.
 * @param app Pointer to the application state.
 */
void app_draw_gamepad_help_overlay(const App* app);

/**
 * @brief Draws the debug overlay with performance metrics and settings.
 * @param app Pointer to the application state.
 */
void app_draw_debug_overlay(const App* app);

/**
 * @brief Main UI rendering entry point.
 *
 * Orchestrates all UI elements (overlays, histograms, loading spinners).
 * @param app Pointer to the application state.
 */
void app_render_ui(const App* app);

/* --- Internal helper functions --- */

/**
 * @brief Computes the luminance histogram from current frame data.
 *
 * Scans the luminance buffer to populate buckets for visualization.
 * @param app Pointer to the application state.
 * @param buckets Array to fill with histogram data.
 * @param size Number of buckets in the array.
 * @param[out] min_lum Minimum luminance found in the data.
 * @param[out] max_lum Maximum luminance found in the data.
 * @return Number of samples processed or error code.
 */
int compute_luminance_histogram(const App* app, int* buckets, int size,
                                float* min_lum, float* max_lum);

void app_ui_init(AppUIOverlay* overlay);
void app_ui_cleanup(AppUIOverlay* overlay);
void app_ui_update(AppUIOverlay* overlay, double delta_time);
void app_ui_handle_mouse(AppUIOverlay* overlay, double xpos, double ypos,
                         int width, int height);

#endif /* APP_UI_H */
