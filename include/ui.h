/**
 * @file ui.h
 * @brief Minimal immediate-mode User Interface (UI) library.
 *
 * This module provides basic text rendering and shape drawing for overlays,
 * debug information, and menus. It uses a custom font atlas.
 */

#ifndef UI_H
#define UI_H

/** @brief Number of supported glyphs in the font atlas. */
#define ASCII_CHAR_COUNT 96

#include "shader.h"
#include <cglm/cglm.h>

/**
 * @struct GlyphInfo
 * @brief Metrics and texture coordinates for a single font character.
 */
typedef struct {
	float x0, y0, x1, y1; /**< Normalized texture coordinates. */
	float w, h;           /**< Size in pixels. */
	float x_off, y_off;   /**< Rendering offsets. */
	float advance;        /**< Horizontal spacing to the next char. */
} GlyphInfo;

/**
 * @struct UIContext
 * @brief Persistent state for the UI system.
 */
typedef struct {
	GLuint texture; /**< Font atlas texture handle. */
	Shader* shader; /**< Main UI shader. */
	Shader*
	    spinner_shader; /**< Specialized shader for loading animations. */
	GLuint vao, vbo;    /**< Geometry buffers. */
	GlyphInfo cdata[ASCII_CHAR_COUNT]; /**< Metrics for ASCII 32 - 126. */
	float font_size;                   /**< Global scaling factor. */
} UIContext;

/**
 * @struct UILayout
 * @brief Helper for automatic vertical stacking of UI elements.
 */
typedef struct {
	UIContext* ui;     /**< Reference to the UI context. */
	float start_x;     /**< Anchor X position. */
	float cursor_y;    /**< Current vertical insertion point. */
	float padding;     /**< Space between elements. */
	int screen_width;  /**< Current window width. */
	int screen_height; /**< Current window height. */
} UILayout;

/**
 * @brief Initializes the UI system.
 * @param ui_context Pointer to the struct.
 * @param font_path Path to the binary font metrics/texture.
 * @param font_size Default scale.
 * @return 0 on success, non-zero on error.
 */
int ui_init(UIContext* ui_context, const char* font_path, float font_size);

/**
 * @brief Releases UI resources.
 * @param ui_context Pointer to the struct.
 */
void ui_destroy(UIContext* ui_context);

/* --- Layout API --- */

/**
 * @brief Initializes a layout helper for a frame.
 */
void ui_layout_init(UILayout* layout, UIContext* ui_ctx, float pos_x,
                    float pos_y, float padding, int screen_width,
                    int screen_height);

/** @brief Draws text and advances the layout cursor. */
void ui_layout_text(UILayout* layout, const char* text, const vec3 color);

/** @brief Adds vertical space to the layout. */
void ui_layout_separator(UILayout* layout, float space);

/* --- Low-level API --- */

/** @brief Draws a single text string at a specific coordinate. */
void ui_draw_text(UIContext* ui_context, const char* text, float pos_x,
                  float pos_y, const vec3 color, int screen_width,
                  int screen_height);

/** @brief Draws a solid color rectangle. */
void ui_draw_rect(UIContext* ui_context, float rect_x, float rect_y,
                  float width, float height, const vec3 color, int screen_width,
                  int screen_height);

/** @brief Draws a rotating loading spinner. */
void ui_draw_spinner(UIContext* ui_context, float center_x, float center_y,
                     float size, float angle, const vec3 color,
                     int screen_width, int screen_height);

#endif /* UI_H */
