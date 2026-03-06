/**
 * @file app_ui.h
 * @brief UI rendering module for the application.
 *
 * This module handles all UI-related drawing operations, including help
 * overlays, debug information, and luminance statistics.
 */

#ifndef APP_UI_H
#define APP_UI_H
#include "ui.h"

typedef struct {
	float key_size;
	float key_padding;
	float key_radius;
	float label_scale;
	float title_y_offset;
	float detail_y_offset;
} KeyboardLayoutConfig;

typedef struct {
	UIContext ui;
	KeyboardLayoutConfig kbd_config;

	int show_help;
	int show_info_overlay;
	int text_overlay_mode;
	int show_exposure_debug;

	int help_hovered_key;
	int help_pressed_key;
	int help_pressed_mods;
	double help_press_timer;
} AppUIOverlay;

typedef struct App App;

/**
 * @brief Draws the help overlay with keyboard shortcuts.
 * @param app Pointer to the application state.
 */
void app_draw_help_overlay(App* app);

/**
 * @brief Draws the debug overlay with performance metrics and settings.
 * @param app Pointer to the application state.
 */
void app_draw_debug_overlay(App* app);

/**
 * @brief Main UI rendering entry point.
 *
 * Orchestrates all UI elements (overlays, histograms, loading spinners).
 * @param app Pointer to the application state.
 */
void app_render_ui(App* app);

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
int compute_luminance_histogram(App* app, int* buckets, int size,
                                float* min_lum, float* max_lum);

void app_ui_init(AppUIOverlay* overlay);
void app_ui_cleanup(AppUIOverlay* overlay);
void app_ui_update(AppUIOverlay* overlay, double delta_time);

#endif /* APP_UI_H */
