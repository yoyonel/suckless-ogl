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

/** @brief Maximum number of vertices per UI batch. */
#define UI_MAX_BATCH_VERTICES 8192

/**
 * @struct UIVertex
 * @brief Vertex data for the batch renderer.
 */
typedef struct {
	float x, y;
	float u, v;
	float r, g, b, a;
	float mode;  // 0=solid, 1=text, 2=rounded
	float rect_size_x, rect_size_y;
	float radius;
} UIVertex;

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
	UIVertex batch_vertices[UI_MAX_BATCH_VERTICES]; /**< Batch buffer */
	int batch_count;           /**< Current vertex count in batch */
	int current_screen_width;  /**< Screen width for current batch */
	int current_screen_height; /**< Screen height for current batch */
	int batch_active;          /**< Is a batch currently active? */
	GLuint current_texture;    /**< Currently bound texture in the batch */
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

/* --- Batch API --- */

/**
 * @brief Begins a new UI batch.
 * @param ui_context Pointer to the UI context.
 * @param screen_width Current window width.
 * @param screen_height Current window height.
 */
void ui_begin(UIContext* ui_context, int screen_width, int screen_height);

/**
 * @brief Flushes the current UI batch to the GPU.
 * @param ui_context Pointer to the UI context.
 */
void ui_flush(UIContext* ui_context);

/**
 * @brief Ends the current UI batch and restores OpenGL state.
 * @param ui_context Pointer to the UI context.
 */
void ui_end(UIContext* ui_context);

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

/**
 * @brief Measures the width of a text string in pixels.
 * @param ui_context Pointer to the context.
 * @param text The text to measure.
 * @return Width in pixels.
 */
float ui_measure_text(const UIContext* ui_context, const char* text);

/** @brief Draws text with custom alpha transparency. */
void ui_draw_text_ex(UIContext* ui_context, const char* text, float pos_x,
                     float pos_y, const vec3 color, float alpha,
                     int screen_width, int screen_height);

/** @brief Draws text with custom alpha transparency and a scale factor. */
void ui_draw_text_scaled(UIContext* ui_context, const char* text, float pos_x,
                         float pos_y, const vec3 color, float alpha,
                         float scale, int screen_width, int screen_height);

/** @brief Draws a solid color rectangle. */
void ui_draw_rect(UIContext* ui_context, float rect_x, float rect_y,
                  float width, float height, const vec3 color, int screen_width,
                  int screen_height);

/** @brief Draws a solid color rectangle with custom alpha transparency. */
void ui_draw_rect_ex(UIContext* ui_context, float rect_x, float rect_y,
                     float width, float height, const vec3 color, float alpha,
                     int screen_width, int screen_height);

/** @brief Draws a rotating loading spinner. */
void ui_draw_spinner(UIContext* ui_context, float center_x, float center_y,
                     float size, float angle, const vec3 color,
                     int screen_width, int screen_height);

/** @brief Draws a rounded rectangle with custom alpha. */
void ui_draw_rounded_rect(UIContext* ui_context, float rect_x, float rect_y,
                          float width, float height, float radius,
                          const vec3 color, float alpha, int screen_width,
                          int screen_height);

/** @brief Draws an SDF neon glow border around a rounded rectangle area. */
void ui_draw_glow_rect(UIContext* ui_context, float rect_x, float rect_y,
                       float width, float height, float radius,
                       const vec3 color, float alpha, int screen_width,
                       int screen_height);

/**
 * @brief Draws a textured quad tinted by a color (mode 3).
 *
 * The PNG texture's RGB is multiplied by the tint color, and its alpha
 * channel is scaled by the alpha parameter. Use for keycaps and panel frame.
 *
 * @note Flushes the current batch if the active texture differs.
 * @param texture OpenGL texture handle (RGBA PNG).
 * @param tint  Tint color multiplied against the texture RGB.
 * @param alpha Overall opacity.
 */
void ui_draw_textured_quad(UIContext* ui_context, GLuint texture, float rect_x,
                           float rect_y, float width, float height,
                           const vec3 tint, float alpha, int screen_width,
                           int screen_height);

/**
 * @brief Draws a textured quad with additive blending for bloom effects (mode
 * 4).
 *
 * Uses GL_ONE, GL_ONE blending so dark pixels are invisible and bright pixels
 * add light on top of existing content. The texture should be a radial bloom
 * (bright center, black surround). The tint parameter allows coloring the
 * bloom.
 *
 * @param texture OpenGL texture handle (bloom PNG, black background).
 * @param tint    Color of the bloom light (e.g. white or cyan).
 * @param intensity Bloom brightness (0.0 = invisible, 1.0 = full).
 */
void ui_draw_bloom_quad(UIContext* ui_context, GLuint texture, float rect_x,
                        float rect_y, float width, float height,
                        const vec3 tint, float intensity, int screen_width,
                        int screen_height);

#endif /* UI_H */
