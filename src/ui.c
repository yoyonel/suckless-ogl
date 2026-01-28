#include "ui.h"

#include "glad/glad.h"
#include "log.h"
#include "shader.h"
#include <cglm/affine.h>       // NOLINT(misc-include-cleaner)
#include <cglm/cam.h>          // NOLINT(misc-include-cleaner)
#include <cglm/cglm.h>         // NOLINT(misc-include-cleaner)
#include <cglm/struct/mat4.h>  // NOLINT(misc-include-cleaner)
#include <cglm/types.h>        // NOLINT(misc-include-cleaner)
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
	VERTEX_COMPONENTS = 4,  // x, y, u, v
	VERTICES_PER_QUAD = 6,
	FLOATS_PER_VERTEX = 4
};

static const float FONT_ATLAS_SIZE_F = 512.0F;
static const float FONT_BASELINE_OFFSET = 30.0F;
static const size_t MAX_FONT_FILE_SIZE = 10 * 1024 * 1024;  // 10 MB limit

// ============================================================================
// Types
// ============================================================================

typedef struct {
	float x;
	float y;
	float u;
	float v;
} UIVertex;

typedef struct {
	UIVertex vertices[QUAD_VERTICES_COUNT];
} UIQuad;

// ============================================================================
// OpenGL State Management
// ============================================================================

typedef struct {
	GLboolean depth_enabled;
	GLboolean blend_enabled;
	GLint polygon_mode[2];
} GLStateBackup;

static GLStateBackup save_gl_state(void)
{
	GLStateBackup state;
	state.depth_enabled = glIsEnabled(GL_DEPTH_TEST);
	state.blend_enabled = glIsEnabled(GL_BLEND);
	glGetIntegerv(GL_POLYGON_MODE, state.polygon_mode);
	return state;
}

static void restore_gl_state(const GLStateBackup* state)
{
	if (state->depth_enabled != 0U) {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}

	if (state->blend_enabled != 0U) {
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}

	glPolygonMode(GL_FRONT_AND_BACK, state->polygon_mode[0]);
}

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

static unsigned char* read_font_file(const char* path, size_t* out_size)
{
	FILE* file = fopen(path, "rb");
	if (file == NULL) {
		LOG_ERROR("ui", "Failed to open font file: %s", path);
		return NULL;
	}

	(void)fseek(file, 0, SEEK_END);
	const long file_size = ftell(file);
	(void)fseek(file, 0, SEEK_SET);

	if (file_size <= 0 || (size_t)file_size > MAX_FONT_FILE_SIZE) {
		LOG_ERROR("ui", "Invalid font file size: %ld bytes", file_size);
		(void)fclose(file);
		return NULL;
	}

	unsigned char* buffer = malloc((size_t)file_size);
	if (buffer == NULL) {
		LOG_ERROR("ui", "Failed to allocate font buffer");
		(void)fclose(file);
		return NULL;
	}

	const size_t bytes_read = fread(buffer, 1, (size_t)file_size, file);
	(void)fclose(file);

	if (bytes_read != (size_t)file_size) {
		LOG_ERROR("ui", "Failed to read complete font file");
		free(buffer);
		return NULL;
	}

	*out_size = (size_t)file_size;
	return buffer;
}

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

	const GLsizeiptr vbo_size = (GLsizeiptr)sizeof(UIQuad);
	glBufferData(GL_ARRAY_BUFFER, vbo_size, NULL, GL_DYNAMIC_DRAW);

	// Position (x, y) + TexCoords (u, v)
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, FLOATS_PER_VERTEX, GL_FLOAT, GL_FALSE,
	                      (GLsizei)(FLOATS_PER_VERTEX * sizeof(float)),
	                      (void*)0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return 1;
}

// ============================================================================
// Glyph Quad Generation
// ============================================================================

static UIQuad make_glyph_quad(const GlyphInfo* glyph, float render_x,
                              float render_y)
{
	const float width = glyph->w;
	const float height = glyph->h;
	const float left = render_x;
	const float top = render_y;
	const float right = render_x + width;
	const float bottom = render_y + height;

	UIQuad quad = {
	    .vertices = {
	        // Triangle 1
	        {left, bottom, glyph->x0, glyph->y1},  // Bottom-left
	        {left, top, glyph->x0, glyph->y0},     // Top-left
	        {right, top, glyph->x1, glyph->y0},    // Top-right

	        // Triangle 2
	        {left, bottom, glyph->x0, glyph->y1},  // Bottom-left
	        {right, top, glyph->x1, glyph->y0},    // Top-right
	        {right, bottom, glyph->x1, glyph->y1}  // Bottom-right
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
	ui_context->vao = 0;
	ui_context->vbo = 0;
	ui_context->font_size = font_size;
	for (int i = 0; i < FONT_CHAR_COUNT; i++) {
		ui_context->cdata[i] = (GlyphInfo){0};
	}

	// Load font file
	size_t font_buffer_size = 0;
	unsigned char* font_buffer =
	    read_font_file(font_path, &font_buffer_size);
	if (font_buffer == NULL) {
		return 0;
	}

	// Create font atlas
	if (!create_font_atlas(font_buffer, font_size, ui_context)) {
		free(font_buffer);
		return 0;
	}
	free(font_buffer);

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

	LOG_INFO("ui", "UI system initialized successfully");
	return 1;
}

void ui_draw_text(UIContext* ui_context, const char* text, float pos_x,
                  float pos_y, const vec3 color, int screen_width,
                  int screen_height)
{
	if (ui_context == NULL || text == NULL || ui_context->shader == NULL) {
		return;
	}

	// Save and setup OpenGL state
	const GLStateBackup saved_state = save_gl_state();
	setup_ui_render_state();

	// Activate shader
	shader_use(ui_context->shader);

	// Setup orthographic projection
	mat4 projection;  // NOLINT(misc-include-cleaner)
	glm_ortho(0.0F, (float)screen_width, (float)screen_height, 0.0F, -1.0F,
	          1.0F, projection);

	// Upload uniforms
	shader_set_mat4(ui_context->shader, "projection", (float*)projection);
	shader_set_vec3(ui_context->shader, "textColor", (float*)color);
	shader_set_int(ui_context->shader, "useTexture",
	               1); /* Enable Texture for Text */

	// Bind texture and vertex array
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ui_context->texture);
	glBindVertexArray(ui_context->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_context->vbo);

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
		const float render_x = current_x + glyph->x_off;
		const float render_y =
		    pos_y + glyph->y_off + FONT_BASELINE_OFFSET;

		// Generate and upload quad
		const UIQuad quad = make_glyph_quad(glyph, render_x, render_y);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(UIQuad), &quad);

		// Draw
		glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_QUAD);

		// Advance cursor
		current_x += glyph->advance;
	}

	// Cleanup
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	// Restore OpenGL state
	restore_gl_state(&saved_state);
}

// NOLINTNEXTLINE(readability-identifier-length)
void ui_draw_rect(UIContext* ui_context, float rect_x, float rect_y,
                  float width, float height, const vec3 color, int screen_width,
                  int screen_height)
{
	if (ui_context == NULL || ui_context->shader == NULL) {
		return;
	}

	// Save and setup OpenGL state
	const GLStateBackup saved_state = save_gl_state();
	setup_ui_render_state();

	// Activate shader
	shader_use(ui_context->shader);

	// Setup orthographic projection
	mat4 projection;  // NOLINT(misc-include-cleaner)
	glm_ortho(0.0F, (float)screen_width, (float)screen_height, 0.0F, -1.0F,
	          1.0F, projection);

	// Upload uniforms
	shader_set_mat4(ui_context->shader, "projection", (float*)projection);
	shader_set_vec3(ui_context->shader, "textColor", (float*)color);
	shader_set_int(ui_context->shader, "useTexture",
	               0); /* Disable Texture for Rect */

	// Bind vertex array (No texture binding needed, but VAO is required)
	glBindVertexArray(ui_context->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_context->vbo);

	/* Construct Quad manually */
	UIQuad quad = {
	    .vertices = {
	        // Triangle 1
	        {rect_x, rect_y + height, 0.0F, 0.0F},  // Bottom-left
	        {rect_x, rect_y, 0.0F, 0.0F},           // Top-left
	        {rect_x + width, rect_y, 0.0F, 0.0F},   // Top-right

	        // Triangle 2
	        {rect_x, rect_y + height, 0.0F, 0.0F},         // Bottom-left
	        {rect_x + width, rect_y, 0.0F, 0.0F},          // Top-right
	        {rect_x + width, rect_y + height, 0.0F, 0.0F}  // Bottom-right
	    }};

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(UIQuad), &quad);
	glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_QUAD);

	// Cleanup
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);

	// Restore OpenGL state
	restore_gl_state(&saved_state);
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
