#ifndef UI_H
#define UI_H

#include "glad/glad.h"
#include <cglm/cglm.h>

typedef struct {
	float x0, y0, x1, y1;  // Coordonnées texture
	float w, h;            // Taille du caractère
	float x_off, y_off;    // Offsets de rendu
	float advance;         // Espacement horizontal
} GlyphInfo;

typedef struct {
	GLuint texture;
	GLuint shader;
	GLuint vao, vbo;
	GlyphInfo cdata[96];  // Pour les caractères ASCII 32 à 126
} UIContext;

int ui_init(UIContext* ui_context, const char* font_path, float font_size);
void ui_draw_text(UIContext* ui_context, const char* text, float x, float y,
                  vec3 color, int screen_width, int screen_height);
void ui_destroy(UIContext* ui_context);

#endif