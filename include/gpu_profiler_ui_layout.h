/**
 * @file gpu_profiler_ui_layout.h
 * @brief Private UI layout constants for the GPU profiler overlay.
 *
 * This header is included ONLY by gpu_profiler_ui.c. It holds visual
 * tuning constants (font sizes, padding, alpha values, thresholds) that
 * do not belong in the public API.
 */

#ifndef GPU_PROFILER_UI_LAYOUT_H
#define GPU_PROFILER_UI_LAYOUT_H

/* Graph and Font */
static const float GRAPH_FONT_SIZE = 20.0F;
static const float ROW_PAD = 3.0F;
static const float BG_ALPHA = 0.85F;
static const float PAD_SIDE = 20.0F;
static const float GRAPH_WIDTH_RATIO = 0.65F;
static const float TEXT_GAP = 20.0F;
static const float MARGIN_Y = 60.0F;

/* Background Panel */
static const float BG_RADIUS = 8.0F;
static const float BG_PAD = 5.0F;
static const float BG_WIDTH_EXT = 10.0F;
static const float BG_RGB_VAL = 0.05F;

/* Visibility and Fading */
static const float FADE_EPSILON = 0.001F;
static const float VISIBILITY_THRESHOLD = 0.5F;
static const float MIN_VISIBLE_ALPHA_GLOBAL = 0.001F;

/* Bar Graph */
static const float MIN_MS = 0.001F;
static const float MIN_BAR_WIDTH = 1.0F;
static const float BAR_RADIUS_FACTOR = 0.5F;

/* Text Layout */
static const float ROW_PAD_DOUBLE = 2.0F;
static const float INDENT_STEP = 15.0F;
static const float TEXT_Y_OFFSET = -5.0F;
static const float TEXT_SHADOW_OFFSET = 1.0F;

enum { TIME_BUF_SIZE = 32 };

#endif /* GPU_PROFILER_UI_LAYOUT_H */
