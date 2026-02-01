/**
 * @file app_ui.h
 * @brief UI rendering module for the application.
 *
 * This module handles all UI-related drawing operations, including help
 * overlays, debug information, and luminance statistics.
 */

#ifndef APP_UI_H
#define APP_UI_H

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
 * @brief Draws exposure-specific debug text.
 * @param app Pointer to the application state.
 * @note Modifies the current UI context state.
 */
void draw_exposure_debug_text(App* app);

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

/**
 * @brief Renders the luminance histogram as a graph on screen.
 * @param app Pointer to the application state.
 * @param buckets Array containing histogram data.
 * @param size Number of buckets in the array.
 * @param min_lum Minimum luminance value in the histogram.
 * @param max_lum Maximum luminance value in the histogram.
 */
void draw_luminance_histogram_graph(App* app, const int* buckets, int size,
                                    float min_lum, float max_lum);

#endif /* APP_UI_H */
