#ifndef UI_H
#define UI_H

#include "gl_common.h"
#include <cglm/cglm.h>

typedef struct {
	float x0, y0, x1, y1;  // Coordonnées texture
	float w, h;            // Taille du caractère
	float x_off, y_off;    // Offsets de rendu
	float advance;         // Espacement horizontal
} GlyphInfo;

enum FontAtlasConfig {
	FONT_ATLAS_SIZE = 512,
	FONT_FIRST_CHAR = 32,
	FONT_CHAR_COUNT = 96
};

#include "shader.h"

typedef struct {
	GLuint texture;
	Shader* shader;
	GLuint vao, vbo;
	GlyphInfo cdata[FONT_CHAR_COUNT];  // Pour les caractères ASCII 32 à 126
	float font_size;

	/* Async Exposure Readback */
	GLuint exposure_pbo;    /* Pixel Buffer Object for async read */
	float current_exposure; /* CPU-side cached exposure value */
} UIContext;

/* UILayout: Helper for automatic vertical stacking of UI elements */
typedef struct {
	UIContext* ui;
	float start_x;
	float cursor_y;
	float padding;
	int screen_width;
	int screen_height;
} UILayout;

int ui_init(UIContext* ui_context, const char* font_path, float font_size);
void ui_destroy(UIContext* ui_context);

/* Layout API */
void ui_layout_init(UILayout* layout, UIContext* ui_ctx, float start_x,
                    float start_y, float padding, int screen_width,
                    int screen_height);
void ui_layout_text(UILayout* layout, const char* text, const vec3 color);
void ui_layout_separator(UILayout* layout, float space);

/* Low-level API */
void ui_draw_text(UIContext* ui_context, const char* text, float x_pos,
                  float y_pos, const vec3 color, int screen_width,
                  int screen_height);
void ui_draw_rect(UIContext* ui_context, float rect_x, float rect_y,
                  float width, float height, const vec3 color, int screen_width,
                  int screen_height);

/* Exposure Readback */
void ui_update_exposure_readback(UIContext* ui_ctx, GLuint exposure_tex);
float ui_get_exposure(const UIContext* ui_ctx);

#endif
