#include "ui.h"

#include "glad/glad.h"
#include "io.h"
#include "log.h"
#include "render_utils.h"
#include "shader.h"
#include "utils.h"
#include <cglm/affine.h>  // IWYU pragma: keep
#include <cglm/cam.h>     // IWYU pragma: keep
#include <cglm/mat4.h>    // IWYU pragma: keep
#include <cglm/types.h>   // IWYU pragma: keep
#include <cglm/vec3.h>    // IWYU pragma: keep
#include <stb_truetype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Constants
// ============================================================================

enum FontAtlasConfig {
	FONT_ATLAS_SIZE = 512,
	FONT_FIRST_CHAR = 32,
	FONT_CHAR_COUNT = 96
};

enum VertexConfig {
	QUAD_VERTICES_COUNT = 6,
	VERTEX_COMPONENTS = 12,  // x, y, u, v, r, g, b, a, mode, w, h, radius
	VERTICES_PER_QUAD = 6,
	FLOATS_PER_VERTEX = 12
};

enum VertexID {
	VERTEX_0 = 0,
	VERTEX_1,
	VERTEX_2,
	VERTEX_3,
	VERTEX_4,
	VERTEX_5
};

static const float FONT_ATLAS_SIZE_F = 512.0F;

static const float FONT_BASELINE_OFFSET = 30.0F;
static const size_t MAX_FONT_FILE_SIZE = 10 * 1024 * 1024;  // 10 MB limit
static const float UI_QUAD_POS_HALF = 0.5F;
static const float UI_QUAD_TEX_MAX = 1.0F;
static const float UI_QUAD_MIN = 0.0F;

// Modes de rendu pour le shader UI
static const float UI_MODE_SOLID = 0.0F;
static const float UI_MODE_TEXT = 1.0F;
static const float UI_MODE_ROUNDED = 2.0F;
static const float UI_MODE_TEXTURED = 3.0F;
static const float UI_MODE_BLOOM = 4.0F;
static const float UI_MODE_GLOW = 5.0F;

// ============================================================================
// Batch Rendering State
// ============================================================================

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static GLStateBackup g_ui_saved_state;

// ============================================================================
// OpenGL State Management
// ============================================================================

static void setup_ui_render_state(void)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static inline void push_batch_quad(UIVertex* out_vert, float left, float right,
                                   float top, float bottom, float tex_u0,
                                   float tex_v0, float tex_u1, float tex_v1,
                                   float col_r, float col_g, float col_b,
                                   float col_a, float mode, float param_w,
                                   float param_h, float radius)
{
	/* Triangle 1 */
	out_vert[VERTEX_0] =
	    (UIVertex){left,  bottom, tex_u0, tex_v1,  col_r,   col_g,
	               col_b, col_a,  mode,   param_w, param_h, radius};
	out_vert[VERTEX_1] =
	    (UIVertex){left,  top,   tex_u0, tex_v0,  col_r,   col_g,
	               col_b, col_a, mode,   param_w, param_h, radius};
	out_vert[VERTEX_2] =
	    (UIVertex){right, top,   tex_u1, tex_v0,  col_r,   col_g,
	               col_b, col_a, mode,   param_w, param_h, radius};

	/* Triangle 2 */
	out_vert[VERTEX_3] =
	    (UIVertex){left,  bottom, tex_u0, tex_v1,  col_r,   col_g,
	               col_b, col_a,  mode,   param_w, param_h, radius};
	out_vert[VERTEX_4] =
	    (UIVertex){right, top,   tex_u1, tex_v0,  col_r,   col_g,
	               col_b, col_a, mode,   param_w, param_h, radius};

	out_vert[VERTEX_5] =
	    (UIVertex){right, bottom, tex_u1, tex_v1,  col_r,   col_g,
	               col_b, col_a,  mode,   param_w, param_h, radius};
}

// ============================================================================
// Font Loading Helpers
// ============================================================================

static int create_font_atlas(unsigned char* font_buffer, float font_size,
                             UIContext* ui_context)
{
	const size_t bitmap_size = (size_t)(FONT_ATLAS_SIZE * FONT_ATLAS_SIZE);
	unsigned char* bitmap = calloc(bitmap_size, 1);
	if (bitmap == NULL) {
		LOG_ERROR("ui", "Failed to allocate font atlas bitmap");
		return 0;
	}

	stbtt_bakedchar chardata[FONT_CHAR_COUNT];
	const int result = stbtt_BakeFontBitmap(
	    font_buffer, 0, font_size, bitmap, FONT_ATLAS_SIZE, FONT_ATLAS_SIZE,
	    FONT_FIRST_CHAR, FONT_CHAR_COUNT, chardata);

	if (result <= 0) {
		LOG_ERROR("ui", "Failed to bake font bitmap");
		free(bitmap);
		return 0;
	}

	// Create OpenGL texture
	glGenTextures(1, &ui_context->texture);
	glBindTexture(GL_TEXTURE_2D, ui_context->texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_ATLAS_SIZE, FONT_ATLAS_SIZE,
	             0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	free(bitmap);

	// Convert stbtt_bakedchar to GlyphInfo
	for (int i = 0; i < FONT_CHAR_COUNT; i++) {
		ui_context->cdata[i] =
		    (GlyphInfo){.x0 = (float)chardata[i].x0 / FONT_ATLAS_SIZE_F,
		                .y0 = (float)chardata[i].y0 / FONT_ATLAS_SIZE_F,
		                .x1 = (float)chardata[i].x1 / FONT_ATLAS_SIZE_F,
		                .y1 = (float)chardata[i].y1 / FONT_ATLAS_SIZE_F,
		                .w = (float)(chardata[i].x1 - chardata[i].x0),
		                .h = (float)(chardata[i].y1 - chardata[i].y0),
		                .x_off = chardata[i].xoff,
		                .y_off = chardata[i].yoff,
		                .advance = chardata[i].xadvance};
	}

	return 1;
}

static int setup_vertex_buffers(UIContext* ui_context)
{
	glGenVertexArrays(1, &ui_context->vao);
	glGenBuffers(1, &ui_context->vbo);

	glBindVertexArray(ui_context->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_context->vbo);

	const GLsizeiptr vbo_size =
	    (GLsizeiptr)(UI_MAX_BATCH_VERTICES * sizeof(UIVertex));
	glBufferData(GL_ARRAY_BUFFER, vbo_size, NULL, GL_DYNAMIC_DRAW);

	// Ensure layout matches UIVertex exactly (12 floats)
	const GLsizei stride = (GLsizei)(sizeof(UIVertex));

	// Position (x, y)
	glEnableVertexAttribArray(0);
	glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE,
	                     (GLuint)offsetof(UIVertex, x));
	glVertexAttribBinding(0, 0);

	// TexCoords (u, v)
	glEnableVertexAttribArray(1);
	glVertexAttribFormat(1, 2, GL_FLOAT, GL_FALSE,
	                     (GLuint)offsetof(UIVertex, u));
	glVertexAttribBinding(1, 0);

	// Color (r, g, b, a)
	glEnableVertexAttribArray(2);
	glVertexAttribFormat(2, 4, GL_FLOAT, GL_FALSE,
	                     (GLuint)offsetof(UIVertex, r));
	glVertexAttribBinding(2, 0);

	// Mode (1 float)
	glEnableVertexAttribArray(3);
	glVertexAttribFormat(3, 1, GL_FLOAT, GL_FALSE,
	                     (GLuint)offsetof(UIVertex, mode));
	glVertexAttribBinding(3, 0);

	// Rounded params (w, h, radius)
	glEnableVertexAttribArray(4);
	glVertexAttribFormat(4, 3, GL_FLOAT, GL_FALSE,
	                     (GLuint)offsetof(UIVertex, rect_size_x));
	glVertexAttribBinding(4, 0);

	glBindVertexBuffer(0, ui_context->vbo, 0, stride);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return 1;
}

// ============================================================================
// Batch Rendering Helpers
// ============================================================================

static void prepare_batch(UIContext* ui_context, GLuint texture)
{
	if (ui_context->current_texture != texture &&
	    ui_context->batch_count > 0) {
		ui_flush(ui_context);
	}
	ui_context->current_texture = texture;

	if (ui_context->batch_count + VERTICES_PER_QUAD >
	    UI_MAX_BATCH_VERTICES) {
		ui_flush(ui_context);
	}
}

// ============================================================================
// Glyph Quad Generation
// ============================================================================

static void make_glyph_quad(const GlyphInfo* glyph, float render_x,
                            float render_y, float scale, const vec3 color,
                            float alpha, UIVertex* out_vertices)
{
	push_batch_quad(
	    out_vertices, render_x,
	    render_x + (glyph->w * scale),              /* left, right */
	    render_y, render_y + (glyph->h * scale),    /* top, bottom */
	    glyph->x0, glyph->y0, glyph->x1, glyph->y1, /* UVs du glyph */
	    color[0], color[1], color[2], alpha,        /* RGBA */
	    UI_MODE_TEXT, 0.0F, 0.0F, 0.0F /* mode Text, params inutilisés */
	);
}

// ============================================================================
// Public API
// ============================================================================

int ui_init(UIContext* ui_context, const char* font_path, float font_size)
{
	if (ui_context == NULL || font_path == NULL) {
		LOG_ERROR("ui", "Invalid arguments to ui_init");
		return 0;
	}

	// Initialize to safe defaults (manual zeroing to avoid memset warning)
	ui_context->texture = 0;
	ui_context->shader = NULL;
	ui_context->spinner_shader = NULL;
	ui_context->vao = 0;
	ui_context->vbo = 0;
	ui_context->font_size = font_size;
	ui_context->batch_count = 0;
	ui_context->batch_active = false;
	ui_context->current_texture = 0;
	for (int i = 0; i < FONT_CHAR_COUNT; i++) {
		ui_context->cdata[i] = (GlyphInfo){0};
	}

	// Load font file
	size_t font_buffer_size = 0;
	CLEANUP_FREE unsigned char* font_buf = (unsigned char*)io_read_file(
	    font_path, MAX_FONT_FILE_SIZE, &font_buffer_size);
	if (font_buf == NULL) {
		return 0;
	}

	// Create font atlas
	if (!create_font_atlas(font_buf, font_size, ui_context)) {
		/* font_buf is CLEANUP_FREE, no manual free needed */
		RAII_SATISFY_FREE(font_buf);
		return 0;
	}

	// Setup vertex buffers
	if (!setup_vertex_buffers(ui_context)) {
		glDeleteTextures(1, &ui_context->texture);
		return 0;
	}

	// Load shader
	ui_context->shader = shader_load("shaders/ui.vert", "shaders/ui.frag");
	if (ui_context->shader == NULL) {
		LOG_ERROR("ui", "Failed to load UI shader");
		glDeleteTextures(1, &ui_context->texture);
		glDeleteBuffers(1, &ui_context->vbo);
		glDeleteVertexArrays(1, &ui_context->vao);
		return 0;
	}

	ui_context->spinner_shader =
	    shader_load("shaders/ui_spinner.vert", "shaders/ui_spinner.frag");
	if (ui_context->spinner_shader == NULL) {
		LOG_ERROR("ui", "Failed to load UI spinner shader");
		shader_destroy(ui_context->shader);
		glDeleteTextures(1, &ui_context->texture);
		glDeleteBuffers(1, &ui_context->vbo);
		glDeleteVertexArrays(1, &ui_context->vao);
		return 0;
	}

	LOG_INFO("ui", "UI system initialized successfully");
	return 1;
}

void ui_begin(UIContext* ui_context, int screen_width, int screen_height)
{
	if (ui_context == NULL) {
		return;
	}

	if (ui_context->batch_active) {
		LOG_WARNING("ui",
		            "ui_begin called while a batch is already active.");
		return;
	}

	g_ui_saved_state = render_utils_save_state();
	setup_ui_render_state();

	ui_context->current_screen_width = screen_width;
	ui_context->current_screen_height = screen_height;
	ui_context->batch_count = 0;
	ui_context->batch_active = true;
	ui_context->current_texture =
	    ui_context->texture; /* Default to font atlas */
}

void ui_flush(UIContext* ui_context)
{
	if (ui_context == NULL || ui_context->batch_count == 0) {
		return;
	}

	shader_use(ui_context->shader);

	mat4 projection;
	glm_ortho(0.0F, (float)ui_context->current_screen_width,
	          (float)ui_context->current_screen_height, 0.0F, -1.0F, 1.0F,
	          projection);

	shader_set_mat4(ui_context->shader, "projection", (float*)projection);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ui_context->current_texture);
	glBindVertexArray(ui_context->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_context->vbo);

	glBufferSubData(
	    GL_ARRAY_BUFFER, 0,
	    (GLsizeiptr)(ui_context->batch_count * sizeof(UIVertex)),
	    ui_context->batch_vertices);

	glDrawArrays(GL_TRIANGLES, 0, ui_context->batch_count);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	ui_context->batch_count = 0;
}

void ui_end(UIContext* ui_context)
{
	if (ui_context == NULL || !ui_context->batch_active) {
		return;
	}

	ui_flush(ui_context);
	render_utils_restore_state(&g_ui_saved_state);
	ui_context->batch_active = false;
}

void ui_draw_text(UIContext* ui_context, const char* text, float pos_x,
                  float pos_y, const vec3 color, int screen_width,
                  int screen_height)
{
	ui_draw_text_ex(ui_context, text, pos_x, pos_y, color, 1.0F,
	                screen_width, screen_height);
}

float ui_measure_text(const UIContext* ui_context, const char* text)
{
	if (ui_context == NULL || text == NULL) {
		return 0.0F;
	}

	float width = 0.0F;
	for (const char* ptr = text; *ptr != '\0'; ptr++) {
		const unsigned char char_code = (unsigned char)*ptr;

		if (char_code < FONT_FIRST_CHAR ||
		    char_code >= (FONT_FIRST_CHAR + FONT_CHAR_COUNT)) {
			continue;
		}

		const GlyphInfo* glyph =
		    &ui_context->cdata[char_code - FONT_FIRST_CHAR];
		width += glyph->advance;
	}

	return width;
}

void ui_draw_text_ex(UIContext* ui_context, const char* text, float pos_x,
                     float pos_y, const vec3 color, float alpha,
                     int screen_width, int screen_height)
{
	ui_draw_text_scaled(ui_context, text, pos_x, pos_y, color, alpha, 1.0F,
	                    screen_width, screen_height);
}

void ui_draw_text_scaled(UIContext* ui_context, const char* text, float pos_x,
                         float pos_y, const vec3 color, float alpha,
                         float scale, int screen_width, int screen_height)
{
	if (ui_context == NULL || text == NULL || ui_context->shader == NULL) {
		return;
	}

	int auto_batch = 0;
	if (!ui_context->batch_active) {
		ui_begin(ui_context, screen_width, screen_height);
		auto_batch = 1;
	}

	// Render each character
	float current_x = pos_x;
	for (const char* ptr = text; *ptr != '\0'; ptr++) {
		const unsigned char char_code = (unsigned char)*ptr;

		if (char_code < FONT_FIRST_CHAR ||
		    char_code >= (FONT_FIRST_CHAR + FONT_CHAR_COUNT)) {
			continue;
		}

		const GlyphInfo* glyph =
		    &ui_context->cdata[char_code - FONT_FIRST_CHAR];

		const float render_x = current_x + (glyph->x_off * scale);
		const float render_y =
		    pos_y + ((glyph->y_off + FONT_BASELINE_OFFSET) * scale);

		prepare_batch(ui_context, ui_context->texture);

		// Appel direct avec l'adresse du prochain emplacement
		// disponible dans le batch
		make_glyph_quad(
		    glyph, render_x, render_y, scale, color, alpha,
		    &ui_context->batch_vertices[ui_context->batch_count]);

		// Avancer le compteur manuellement
		ui_context->batch_count += VERTICES_PER_QUAD;

		current_x += (glyph->advance * scale);
	}

	if (auto_batch) {
		ui_end(ui_context);
	}
}

// NOLINTNEXTLINE(readability-identifier-length)
void ui_draw_rect(UIContext* ui_context, float rect_x, float rect_y,
                  float width, float height, const vec3 color, int screen_width,
                  int screen_height)
{
	ui_draw_rect_ex(ui_context, rect_x, rect_y, width, height, color, 1.0F,
	                screen_width, screen_height);
}

void ui_draw_rect_ex(UIContext* ui_context, float rect_x, float rect_y,
                     float width, float height, const vec3 color, float alpha,
                     int screen_width, int screen_height)
{
	if (ui_context == NULL || ui_context->shader == NULL) {
		return;
	}

	int auto_batch = 0;
	if (!ui_context->batch_active) {
		ui_begin(ui_context, screen_width, screen_height);
		auto_batch = 1;
	}

	prepare_batch(ui_context, ui_context->texture);

	UIVertex* out = &ui_context->batch_vertices[ui_context->batch_count];

	push_batch_quad(out, rect_x, rect_x + width, rect_y, rect_y + height,
	                0.0F, 0.0F, 1.0F, 1.0F, color[0], color[1], color[2],
	                alpha, UI_MODE_SOLID, 0.0F, 0.0F, 0.0F);

	ui_context->batch_count += VERTICES_PER_QUAD;

	if (auto_batch) {
		ui_end(ui_context);
	}
}

void ui_destroy(UIContext* ui_context)
{
	if (ui_context == NULL) {
		return;
	}

	if (ui_context->texture != 0) {
		glDeleteTextures(1, &ui_context->texture);
		ui_context->texture = 0;
	}
	if (ui_context->vbo != 0) {
		glDeleteBuffers(1, &ui_context->vbo);
		ui_context->vbo = 0;
	}
	if (ui_context->vao != 0) {
		glDeleteVertexArrays(1, &ui_context->vao);
		ui_context->vao = 0;
	}
	if (ui_context->shader != 0) {
		shader_destroy(ui_context->shader);
		ui_context->shader = NULL;
	}
	if (ui_context->spinner_shader != 0) {
		shader_destroy(ui_context->spinner_shader);
		ui_context->spinner_shader = NULL;
	}

	LOG_INFO("ui", "UI system destroyed");
}

void ui_layout_init(UILayout* layout, UIContext* ui_ctx, float pos_x,
                    float pos_y, float padding, int screen_width,
                    int screen_height)
{
	layout->ui = ui_ctx;
	layout->start_x = pos_x;
	layout->cursor_y = pos_y;
	layout->padding = padding;
	layout->screen_width = screen_width;
	layout->screen_height = screen_height;
}

void ui_layout_text(UILayout* layout, const char* text, const vec3 color)
{
	if (!layout || !layout->ui) {
		return;
	}

	ui_draw_text(layout->ui, text, layout->start_x, layout->cursor_y, color,
	             layout->screen_width, layout->screen_height);

	/* Advance cursor */
	/* Note: ui->font_size indicates height roughly */
	layout->cursor_y += layout->ui->font_size + layout->padding;
}

void ui_layout_separator(UILayout* layout, float space)
{
	if (!layout) {
		return;
	}
	layout->cursor_y += space;
}

void ui_draw_spinner(UIContext* ui_context, float center_x, float center_y,
                     float size, float angle, const vec3 color,
                     int screen_width, int screen_height)
{
	if (ui_context == NULL || ui_context->spinner_shader == NULL) {
		return;
	}

	// Flush the active batch before changing shader and state
	if (ui_context->batch_active) {
		ui_flush(ui_context);
	}

	const GLStateBackup saved_state = render_utils_save_state();
	setup_ui_render_state();

	shader_use(ui_context->spinner_shader);

	mat4 projection;
	glm_ortho(0.0F, (float)screen_width, (float)screen_height, 0.0F, -1.0F,
	          1.0F, projection);
	shader_set_mat4(ui_context->spinner_shader, "projection",
	                (float*)projection);

	/* Model Matrix Construction (GPU Rotation) */
	mat4 model;
	glm_mat4_identity(model);
	glm_translate(model, (vec3){center_x, center_y, 0.0F});
	glm_rotate(model, angle, (vec3){0.0F, 0.0F, 1.0F});
	glm_scale(model, (vec3){size, size, 1.0F});
	shader_set_mat4(ui_context->spinner_shader, "model", (float*)model);

	shader_set_vec3(ui_context->spinner_shader, "color", (float*)color);

	glBindVertexArray(ui_context->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_context->vbo);

	UIVertex vertices[VERTICES_PER_QUAD] = {
	    {-UI_QUAD_POS_HALF, UI_QUAD_POS_HALF, UI_QUAD_MIN, UI_QUAD_TEX_MAX,
	     0, 0, 0, 0, 0, 0, 0, 0},
	    {-UI_QUAD_POS_HALF, -UI_QUAD_POS_HALF, UI_QUAD_MIN, UI_QUAD_MIN, 0,
	     0, 0, 0, 0, 0, 0, 0},
	    {UI_QUAD_POS_HALF, -UI_QUAD_POS_HALF, UI_QUAD_TEX_MAX, UI_QUAD_MIN,
	     0, 0, 0, 0, 0, 0, 0, 0},
	    {-UI_QUAD_POS_HALF, UI_QUAD_POS_HALF, UI_QUAD_MIN, UI_QUAD_TEX_MAX,
	     0, 0, 0, 0, 0, 0, 0, 0},
	    {UI_QUAD_POS_HALF, -UI_QUAD_POS_HALF, UI_QUAD_TEX_MAX, UI_QUAD_MIN,
	     0, 0, 0, 0, 0, 0, 0, 0},
	    {UI_QUAD_POS_HALF, UI_QUAD_POS_HALF, UI_QUAD_TEX_MAX,
	     UI_QUAD_TEX_MAX, 0, 0, 0, 0, 0, 0, 0, 0}};

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
	glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_QUAD);

	/* Cleanup */
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);

	render_utils_restore_state(&saved_state);
}

// NOLINTNEXTLINE(readability-identifier-length)
void ui_draw_rounded_rect(UIContext* ui_context, float rect_x, float rect_y,
                          float width, float height, float radius,
                          const vec3 color, float alpha, int screen_width,
                          int screen_height)
{
	if (ui_context == NULL || ui_context->shader == NULL) {
		return;
	}

	int auto_batch = 0;
	if (!ui_context->batch_active) {
		ui_begin(ui_context, screen_width, screen_height);
		auto_batch = 1;
	}

	prepare_batch(ui_context, ui_context->texture);

	const float col_r = color[0];
	const float col_g = color[1];
	const float col_b = color[2];
	const float col_a = alpha;
	const float mode = 2.0F; /* Rounded Rect */

	UIVertex* out = &ui_context->batch_vertices[ui_context->batch_count];

	push_batch_quad(out, rect_x, rect_x + width, rect_y,
	                rect_y + height,                     /* coords */
	                0.0F, 0.0F, 1.0F, 1.0F,              /* UVs pleins */
	                color[0], color[1], color[2], alpha, /* RGBA */
	                UI_MODE_ROUNDED, width, height,
	                radius /* mode Rounded et params SDF */
	);

	ui_context->batch_count += VERTICES_PER_QUAD;

	if (auto_batch) {
		ui_end(ui_context);
	}
}

/* -------------------------------------------------------------------------- *
 * Textured quad helpers (PNG-based UI assets)                                *
 * -------------------------------------------------------------------------- */

// NOLINTNEXTLINE(readability-identifier-length)
void ui_draw_textured_quad(UIContext* ui_context, GLuint texture, float rect_x,
                           float rect_y, float width, float height,
                           const vec3 tint, float alpha, int screen_width,
                           int screen_height)
{
	if (ui_context == NULL || ui_context->shader == NULL || texture == 0) {
		return;
	}

	int auto_batch = 0;
	if (!ui_context->batch_active) {
		ui_begin(ui_context, screen_width, screen_height);
		auto_batch = 1;
	}

	prepare_batch(ui_context, texture);

	UIVertex* out = &ui_context->batch_vertices[ui_context->batch_count];

	push_batch_quad(out, rect_x, rect_x + width, rect_y, rect_y + height,
	                0.0F, 0.0F, 1.0F, 1.0F, tint[0], tint[1], tint[2],
	                alpha, UI_MODE_TEXTURED, 0.0F, 0.0F,
	                0.0F /* mode Textured */
	);

	ui_context->batch_count += VERTICES_PER_QUAD;

	if (auto_batch) {
		ui_end(ui_context);
	}
}

// NOLINTNEXTLINE(readability-identifier-length)
void ui_draw_bloom_quad(UIContext* ui_context, GLuint texture, float rect_x,
                        float rect_y, float width, float height,
                        const vec3 tint, float intensity, int screen_width,
                        int screen_height)
{
	if (ui_context == NULL || ui_context->shader == NULL || texture == 0 ||
	    intensity <= 0.0F) {
		return;
	}

	/* Bloom requires additive blending. Flush current batch before state
	 * change. */
	if (ui_context->batch_active && ui_context->batch_count > 0) {
		ui_flush(ui_context);
	}

	/* Specialized short-lived batch for bloom */
	const GLStateBackup saved_state = render_utils_save_state();
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	ui_context->current_texture = texture;

	const float col_r = tint[0];
	const float col_g = tint[1];
	const float col_b = tint[2];
	const float col_a = intensity;
	const float mode = 4.0F; /* Textured Additive */

	UIVertex* out = &ui_context->batch_vertices[ui_context->batch_count];

	push_batch_quad(out, rect_x, rect_x + width, rect_y, rect_y + height,
	                0.0F, 0.0F, 1.0F, 1.0F, tint[0], tint[1], tint[2],
	                intensity, UI_MODE_BLOOM, 0.0F, 0.0F, 0.0F);

	ui_context->batch_count += VERTICES_PER_QUAD;

	ui_flush(ui_context);

	render_utils_restore_state(&saved_state);
	ui_context->current_texture = ui_context->texture; /* restore atlas */
}

// NOLINTNEXTLINE(readability-identifier-length)
void ui_draw_glow_rect(UIContext* ui_context, float rect_x, float rect_y,
                       float width, float height, float radius,
                       const vec3 color, float alpha, int screen_width,
                       int screen_height)
{
	if (ui_context == NULL || ui_context->shader == NULL) {
		return;
	}

	int auto_batch = 0;
	if (!ui_context->batch_active) {
		ui_begin(ui_context, screen_width, screen_height);
		auto_batch = 1;
	}

	prepare_batch(ui_context, ui_context->texture);

	UIVertex* out = &ui_context->batch_vertices[ui_context->batch_count];

	push_batch_quad(out, rect_x, rect_x + width, rect_y, rect_y + height,
	                0.0F, 0.0F, 1.0F, 1.0F, color[0], color[1], color[2],
	                alpha, UI_MODE_GLOW, width, height,
	                radius /* mode Glow */
	);

	ui_context->batch_count += VERTICES_PER_QUAD;

	if (auto_batch) {
		ui_end(ui_context);
	}
}
