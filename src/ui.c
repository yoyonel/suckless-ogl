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
#include <stdint.h>
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

static const float FONT_ATLAS_SIZE_F = 512.0F;
static const float FONT_BASELINE_OFFSET = 30.0F;
static const size_t MAX_FONT_FILE_SIZE = 10 * 1024 * 1024;  // 10 MB limit
static const float UI_QUAD_POS_HALF = 0.5F;
static const float UI_QUAD_TEX_MAX = 1.0F;
static const float UI_QUAD_MIN = 0.0F;

// ============================================================================
// ============================================================================

typedef struct {
	UIVertex vertices[QUAD_VERTICES_COUNT];
} UIQuad;

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
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
	                      (void*)offsetof(UIVertex, x));

	// TexCoords (u, v)
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
	                      utils_buffer_offset(offsetof(UIVertex, u)));

	// Color (r, g, b, a)
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
	                      utils_buffer_offset(offsetof(UIVertex, r)));

	// Mode (1 float)
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
	                      utils_buffer_offset(offsetof(UIVertex, mode)));

	// Rounded params (w, h, radius)
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(
	    4, 3, GL_FLOAT, GL_FALSE, stride,
	    utils_buffer_offset(offsetof(UIVertex, rect_size_x)));

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return 1;
}

// ============================================================================
// Glyph Quad Generation
// ============================================================================

static UIQuad make_glyph_quad(const GlyphInfo* glyph, float render_x,
                              float render_y, float scale, const vec3 color,
                              float alpha)
{
	const float width = glyph->w * scale;
	const float height = glyph->h * scale;
	const float left = render_x;
	const float top = render_y;
	const float right = render_x + width;
	const float bottom = render_y + height;

	const float col_r = color[0];
	const float col_g = color[1];
	const float col_b = color[2];
	const float col_a = alpha;
	const float mode = 1.0F; /* Text */

	UIQuad quad = {
	    .vertices = {
	        /* Triangle 1 */
	        {left, bottom, glyph->x0, glyph->y1, col_r, col_g, col_b, col_a,
	         mode, 0.0F, 0.0F, 0.0F}, /* Bottom-left */
	        {left, top, glyph->x0, glyph->y0, col_r, col_g, col_b, col_a,
	         mode, 0.0F, 0.0F, 0.0F}, /* Top-left */
	        {right, top, glyph->x1, glyph->y0, col_r, col_g, col_b, col_a,
	         mode, 0.0F, 0.0F, 0.0F}, /* Top-right */

	        /* Triangle 2 */
	        {left, bottom, glyph->x0, glyph->y1, col_r, col_g, col_b, col_a,
	         mode, 0.0F, 0.0F, 0.0F}, /* Bottom-left */
	        {right, top, glyph->x1, glyph->y0, col_r, col_g, col_b, col_a,
	         mode, 0.0F, 0.0F, 0.0F}, /* Top-right */
	        {right, bottom, glyph->x1, glyph->y1, col_r, col_g, col_b,
	         col_a, mode, 0.0F, 0.0F, 0.0F} /* Bottom-right */
	    }};

	return quad;
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
	ui_context->batch_active = 0;
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
	ui_context->batch_active = 1;
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
	glBindTexture(GL_TEXTURE_2D, ui_context->texture);
	glBindVertexArray(ui_context->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_context->vbo);

	glBufferSubData(
	    GL_ARRAY_BUFFER, 0,
	    (GLsizeiptr)(ui_context->batch_count * sizeof(UIVertex)),
	    ui_context->batch_vertices);

	glDrawArrays(GL_TRIANGLES, 0, ui_context->batch_count);

	glBindVertexArray(0);
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
	ui_context->batch_active = 0;
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

		// Skip characters outside supported range
		if (char_code < FONT_FIRST_CHAR ||
		    char_code >= (FONT_FIRST_CHAR + FONT_CHAR_COUNT)) {
			continue;
		}

		const GlyphInfo* glyph =
		    &ui_context->cdata[char_code - FONT_FIRST_CHAR];

		// Calculate render position
		const float render_x = current_x + (glyph->x_off * scale);
		const float render_y =
		    pos_y + ((glyph->y_off + FONT_BASELINE_OFFSET) * scale);

		// Check if batch is full
		if (ui_context->batch_count + VERTICES_PER_QUAD >
		    UI_MAX_BATCH_VERTICES) {
			ui_flush(ui_context);
		}

		// Generate and append quad
		const UIQuad quad = make_glyph_quad(glyph, render_x, render_y,
		                                    scale, color, alpha);
		for (int i = 0; i < VERTICES_PER_QUAD; i++) {
			ui_context->batch_vertices[ui_context->batch_count++] =
			    quad.vertices[i];
		}

		// Advance cursor
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

	if (ui_context->batch_count + VERTICES_PER_QUAD >
	    UI_MAX_BATCH_VERTICES) {
		ui_flush(ui_context);
	}

	const float col_r = color[0];
	const float col_g = color[1];
	const float col_b = color[2];
	const float col_a = alpha;
	const float mode = 0.0F; /* Solid */

	/* Construct Quad manually */
	UIQuad quad = {
	    .vertices = {
	        /* Triangle 1 */
	        {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g, col_b,
	         col_a, mode, 0.0F, 0.0F, 0.0F}, /* Bottom-left */
	        {rect_x, rect_y, 0.0F, 0.0F, col_r, col_g, col_b, col_a, mode,
	         0.0F, 0.0F, 0.0F}, /* Top-left */
	        {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g, col_b, col_a,
	         mode, 0.0F, 0.0F, 0.0F}, /* Top-right */

	        /* Triangle 2 */
	        {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g, col_b,
	         col_a, mode, 0.0F, 0.0F, 0.0F}, /* Bottom-left */
	        {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g, col_b, col_a,
	         mode, 0.0F, 0.0F, 0.0F}, /* Top-right */
	        {rect_x + width, rect_y + height, 1.0F, 1.0F, col_r, col_g,
	         col_b, col_a, mode, 0.0F, 0.0F, 0.0F} /* Bottom-right */
	    }};

	for (int i = 0; i < VERTICES_PER_QUAD; i++) {
		ui_context->batch_vertices[ui_context->batch_count++] =
		    quad.vertices[i];
	}

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
	// NOLINTNEXTLINE(misc-include-cleaner)
	glm_translate(model, (vec3){center_x, center_y, 0.0F});
	// NOLINTNEXTLINE(misc-include-cleaner)
	glm_rotate(model, angle, (vec3){0.0F, 0.0F, 1.0F});
	// NOLINTNEXTLINE(misc-include-cleaner)
	glm_scale(model, (vec3){size, size, 1.0F});
	shader_set_mat4(ui_context->spinner_shader, "model", (float*)model);

	shader_set_vec3(ui_context->spinner_shader, "color", (float*)color);

	glBindVertexArray(ui_context->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_context->vbo);

	// Construct the unit quad using UIVertex layout (only pos and tex
	// matter here, color is uniform for spinner, mode etc are ignored by
	// spinner shader) We just zero out the rest
	UIQuad quad = {
	    .vertices = {
	        /* Triangle 1 */
	        {-UI_QUAD_POS_HALF, UI_QUAD_POS_HALF, UI_QUAD_MIN,
	         UI_QUAD_TEX_MAX, 0, 0, 0, 0, 0, 0, 0, 0}, /* TL */
	        {-UI_QUAD_POS_HALF, -UI_QUAD_POS_HALF, UI_QUAD_MIN, UI_QUAD_MIN,
	         0, 0, 0, 0, 0, 0, 0, 0}, /* BL */
	        {UI_QUAD_POS_HALF, -UI_QUAD_POS_HALF, UI_QUAD_TEX_MAX,
	         UI_QUAD_MIN, 0, 0, 0, 0, 0, 0, 0, 0}, /* BR */

	        /* Triangle 2 */
	        {-UI_QUAD_POS_HALF, UI_QUAD_POS_HALF, UI_QUAD_MIN,
	         UI_QUAD_TEX_MAX, 0, 0, 0, 0, 0, 0, 0, 0}, /* TL */
	        {UI_QUAD_POS_HALF, -UI_QUAD_POS_HALF, UI_QUAD_TEX_MAX,
	         UI_QUAD_MIN, 0, 0, 0, 0, 0, 0, 0, 0}, /* BR */
	        {UI_QUAD_POS_HALF, UI_QUAD_POS_HALF, UI_QUAD_TEX_MAX,
	         UI_QUAD_TEX_MAX, 0, 0, 0, 0, 0, 0, 0, 0} /* TR */
	    }};

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(UIQuad), &quad);
	glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_QUAD);

	/* Cleanup */
	glBindVertexArray(0);
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

	if (ui_context->batch_count + VERTICES_PER_QUAD >
	    UI_MAX_BATCH_VERTICES) {
		ui_flush(ui_context);
	}

	const float col_r = color[0];
	const float col_g = color[1];
	const float col_b = color[2];
	const float col_a = alpha;
	const float mode = 2.0F; /* Rounded Rect */

	/* Construct Quad manually */
	UIQuad quad = {
	    .vertices = {
	        /* Triangle 1 */
	        {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g, col_b,
	         col_a, mode, width, height, radius}, /* Bottom-left */
	        {rect_x, rect_y, 0.0F, 0.0F, col_r, col_g, col_b, col_a, mode,
	         width, height, radius}, /* Top-left */
	        {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g, col_b, col_a,
	         mode, width, height, radius}, /* Top-right */

	        /* Triangle 2 */
	        {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g, col_b,
	         col_a, mode, width, height, radius}, /* Bottom-left */
	        {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g, col_b, col_a,
	         mode, width, height, radius}, /* Top-right */
	        {rect_x + width, rect_y + height, 1.0F, 1.0F, col_r, col_g,
	         col_b, col_a, mode, width, height, radius} /* Bottom-right */
	    }};

	for (int i = 0; i < VERTICES_PER_QUAD; i++) {
		ui_context->batch_vertices[ui_context->batch_count++] =
		    quad.vertices[i];
	}

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

	/* Flush the current batch (uses font atlas) before switching texture */
	if (ui_context->batch_active && ui_context->batch_count > 0) {
		ui_flush(ui_context);
	}

	int auto_batch = 0;
	if (!ui_context->batch_active) {
		ui_begin(ui_context, screen_width, screen_height);
		auto_batch = 1;
	}

	/* Swap out the font atlas for the PNG texture for this draw */
	shader_use(ui_context->shader);
	mat4 projection;
	glm_ortho(0.0F, (float)screen_width, (float)screen_height, 0.0F, -1.0F,
	          1.0F, projection);
	shader_set_mat4(ui_context->shader, "projection", (float*)projection);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(ui_context->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_context->vbo);

	const float col_r = tint[0];
	const float col_g = tint[1];
	const float col_b = tint[2];
	const float col_a = alpha;
	const float mode = 3.0F; /* Textured Tinted */

	UIQuad quad = {
	    .vertices = {
	        /* Triangle 1 */
	        {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g, col_b,
	         col_a, mode, 0.0F, 0.0F, 0.0F}, /* Bottom-left */
	        {rect_x, rect_y, 0.0F, 0.0F, col_r, col_g, col_b, col_a, mode,
	         0.0F, 0.0F, 0.0F}, /* Top-left */
	        {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g, col_b, col_a,
	         mode, 0.0F, 0.0F, 0.0F}, /* Top-right */
	        /* Triangle 2 */
	        {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g, col_b,
	         col_a, mode, 0.0F, 0.0F, 0.0F}, /* Bottom-left */
	        {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g, col_b, col_a,
	         mode, 0.0F, 0.0F, 0.0F}, /* Top-right */
	        {rect_x + width, rect_y + height, 1.0F, 1.0F, col_r, col_g,
	         col_b, col_a, mode, 0.0F, 0.0F, 0.0F} /* Bottom-right */
	    }};

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(UIQuad), &quad);
	glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_QUAD);

	/* Restore font atlas */
	glBindTexture(GL_TEXTURE_2D, ui_context->texture);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);

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

	/* Flush any pending batch before changing blend mode */
	if (ui_context->batch_active && ui_context->batch_count > 0) {
		ui_flush(ui_context);
	}

	const GLStateBackup saved_state = render_utils_save_state();
	/* Additive blending: bright pixels add light, black = invisible */
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_DEPTH_TEST);

	shader_use(ui_context->shader);
	mat4 projection;
	glm_ortho(0.0F, (float)screen_width, (float)screen_height, 0.0F, -1.0F,
	          1.0F, projection);
	shader_set_mat4(ui_context->shader, "projection", (float*)projection);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(ui_context->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_context->vbo);

	const float col_r = tint[0];
	const float col_g = tint[1];
	const float col_b = tint[2];
	const float col_a = intensity; /* Bloom intensity in alpha */
	const float mode = 4.0F;       /* Textured Additive */

	UIQuad quad = {.vertices = {
	                   /* Triangle 1 */
	                   {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g,
	                    col_b, col_a, mode, 0.0F, 0.0F, 0.0F},
	                   {rect_x, rect_y, 0.0F, 0.0F, col_r, col_g, col_b,
	                    col_a, mode, 0.0F, 0.0F, 0.0F},
	                   {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g,
	                    col_b, col_a, mode, 0.0F, 0.0F, 0.0F},
	                   /* Triangle 2 */
	                   {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g,
	                    col_b, col_a, mode, 0.0F, 0.0F, 0.0F},
	                   {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g,
	                    col_b, col_a, mode, 0.0F, 0.0F, 0.0F},
	                   {rect_x + width, rect_y + height, 1.0F, 1.0F, col_r,
	                    col_g, col_b, col_a, mode, 0.0F, 0.0F, 0.0F}}};

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(UIQuad), &quad);
	glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_QUAD);

	/* Cleanup */
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D,
	              ui_context->texture); /* restore font atlas */
	glUseProgram(0);

	render_utils_restore_state(&saved_state);
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

	if (ui_context->batch_count + VERTICES_PER_QUAD >
	    UI_MAX_BATCH_VERTICES) {
		ui_flush(ui_context);
	}

	const float col_r = color[0];
	const float col_g = color[1];
	const float col_b = color[2];
	const float col_a = alpha;
	const float mode = 5.0F; /* SDF Neon Glow Border */

	/* Construct Quad manually */
	UIQuad quad = {
	    .vertices = {
	        /* Triangle 1 */
	        {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g, col_b,
	         col_a, mode, width, height, radius}, /* Bottom-left */
	        {rect_x, rect_y, 0.0F, 0.0F, col_r, col_g, col_b, col_a, mode,
	         width, height, radius}, /* Top-left */
	        {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g, col_b, col_a,
	         mode, width, height, radius}, /* Top-right */

	        /* Triangle 2 */
	        {rect_x, rect_y + height, 0.0F, 1.0F, col_r, col_g, col_b,
	         col_a, mode, width, height, radius}, /* Bottom-left */
	        {rect_x + width, rect_y, 1.0F, 0.0F, col_r, col_g, col_b, col_a,
	         mode, width, height, radius}, /* Top-right */
	        {rect_x + width, rect_y + height, 1.0F, 1.0F, col_r, col_g,
	         col_b, col_a, mode, width, height, radius} /* Bottom-right */
	    }};

	for (int i = 0; i < VERTICES_PER_QUAD; i++) {
		ui_context->batch_vertices[ui_context->batch_count++] =
		    quad.vertices[i];
	}

	if (auto_batch) {
		ui_end(ui_context);
	}
}
