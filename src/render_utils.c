#include "render_utils.h"

#include "gl_common.h"
#include "log.h"
#include "utils.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Texture Management
// -----------------------------------------------------------------------------

GLuint render_utils_create_color_texture(float red, float green, float blue,
                                         float alpha)
{
	// Setup debug label based on color
	const char* label = "Dummy Color";
	if (red == 0 && green == 0 && blue == 0) {
		label = "Dummy Black";
	} else if (red == 1 && green == 1 && blue == 1) {
		label = "Dummy White";
	}

	GLuint tex = render_utils_create_texture_2d(1, 1, GL_RGBA16F, 1, label);

	glBindTexture(GL_TEXTURE_2D, tex);
	float color[4] = {red, green, blue, alpha};
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, color);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	return tex;
}

void render_utils_bind_texture_safe(GLenum unit, GLuint texture,
                                    GLuint fallback_tex)
{
	glActiveTexture(unit);
	if (texture != 0) {
		glBindTexture(GL_TEXTURE_2D, texture);
	} else {
		glBindTexture(GL_TEXTURE_2D, fallback_tex);
	}
}

void render_utils_reset_texture_units(int start_unit, int end_unit,
                                      GLuint fallback_tex)
{
	for (int i = start_unit; i < end_unit; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, fallback_tex);
	}
	// Always reset active texture to 0 to avoid side effects
	glActiveTexture(GL_TEXTURE0);
}

// -----------------------------------------------------------------------------
// Geometry Helpers
// -----------------------------------------------------------------------------

void render_utils_create_empty_vao(GLuint* vao)
{
	glGenVertexArrays(1, vao);
	glBindVertexArray(*vao);
	glObjectLabel(GL_VERTEX_ARRAY, *vao, -1, "Empty VAO");
	glBindVertexArray(0);
}

void render_utils_create_quad_vbo(GLuint* vbo)
{
	static const float quadVertices[] = {
	    -0.5F, 0.5F, 0.0F, -0.5F, -0.5F, 0.0F,
	    0.5F,  0.5F, 0.0F, 0.5F,  -0.5F, 0.0F,
	};

	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
	             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glObjectLabel(GL_BUFFER, *vbo, -1, "Quad VBO");
}

void render_utils_create_wire_cube_vbo(GLuint* vbo)
{
	static const float cubeVertices[] = {
	    // Bottom face
	    -1.0F, -1.0F, -1.0F, 1.0F, -1.0F, -1.0F, 1.0F, -1.0F, -1.0F, 1.0F,
	    -1.0F, 1.0F, 1.0F, -1.0F, 1.0F, -1.0F, -1.0F, 1.0F, -1.0F, -1.0F,
	    1.0F, -1.0F, -1.0F, -1.0F,

	    // Top face
	    -1.0F, 1.0F, -1.0F, 1.0F, 1.0F, -1.0F, 1.0F, 1.0F, -1.0F, 1.0F,
	    1.0F, 1.0F, 1.0F, 1.0F, 1.0F, -1.0F, 1.0F, 1.0F, -1.0F, 1.0F, 1.0F,
	    -1.0F, 1.0F, -1.0F,

	    // Connections
	    -1.0F, -1.0F, -1.0F, -1.0F, 1.0F, -1.0F, 1.0F, -1.0F, -1.0F, 1.0F,
	    1.0F, -1.0F, 1.0F, -1.0F, 1.0F, 1.0F, 1.0F, 1.0F, -1.0F, -1.0F,
	    1.0F, -1.0F, 1.0F, 1.0F};

	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices,
	             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glObjectLabel(GL_BUFFER, *vbo, -1, "Wire Cube VBO");
}

void render_utils_create_wire_quad_vbo(GLuint* vbo)
{
	static const float quadVertices[] = {-0.5F, 0.5F,  0.0F,  0.5F,
	                                     0.5F,  0.0F,  0.5F,  -0.5F,
	                                     0.0F,  -0.5F, -0.5F, 0.0F};

	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
	             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glObjectLabel(GL_BUFFER, *vbo, -1, "Wire Quad VBO");
}

void render_utils_create_fullscreen_quad(GLuint* vao, GLuint* vbo)
{
	enum { TEX_COORD_OFFSET = 8 };
	static const float
	    screen_quad_vertices[SCREEN_QUAD_VERTEX_COUNT * (2 + 2)] = {
	        /* positions     texCoords */
	        -1.0F, 1.0F, 0.0F, 1.0F,  -1.0F, -1.0F,
	        0.0F,  0.0F, 1.0F, -1.0F, 1.0F,  0.0F,

	        -1.0F, 1.0F, 0.0F, 1.0F,  1.0F,  -1.0F,
	        1.0F,  0.0F, 1.0F, 1.0F,  1.0F,  1.0F};

	glGenVertexArrays(1, vao);
	glGenBuffers(1, vbo);

	glBindVertexArray(*vao);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(screen_quad_vertices),
	             screen_quad_vertices, GL_STATIC_DRAW);

	/* Position */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
	                      (void*)0);
	glVertexAttribDivisor(0, 0);

	/* TexCoords */
	glEnableVertexAttribArray(1);
	const void* tex_offset = utils_buffer_offset(TEX_COORD_OFFSET);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
	                      tex_offset);
	glVertexAttribDivisor(1, 0);

	glBindVertexArray(0);

	glObjectLabel(GL_VERTEX_ARRAY, *vao, -1, "Fullscreen Quad VAO");
}

// -----------------------------------------------------------------------------
// Debugging / Validation
// -----------------------------------------------------------------------------

int render_utils_check_framebuffer(const char* label)
{
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR("render_utils", "Framebuffer incomplete: %s", label);
		return 0;
	}
	return 1;
}

GPUInfo render_utils_get_gpu_info(void)
{
	GPUInfo info;
	info.vendor = (const char*)glGetString(GL_VENDOR);
	info.renderer = (const char*)glGetString(GL_RENDERER);
	info.version = (const char*)glGetString(GL_VERSION);
	return info;
}

static void append_sanitized_char(char raw_char, char* buffer, size_t* dst_idx,
                                  size_t size)
{
	if (*dst_idx >= (size - 1)) {
		return;
	}

	unsigned char unsigned_char = (unsigned char)raw_char;
	if (isalnum(unsigned_char)) {
		buffer[(*dst_idx)++] = (char)tolower((int)unsigned_char);
		return;
	}

	// Handle separators: convert to underscore, but avoid leading or
	// consecutive underscores
	bool is_sep = (unsigned_char == ' ' || unsigned_char == '_' ||
	               unsigned_char == '-' || unsigned_char == '.') != 0;
	if (is_sep && *dst_idx > 0 && buffer[*dst_idx - 1] != '_') {
		buffer[(*dst_idx)++] = '_';
	}
}

enum { GPU_IDENTIFIER_RAW_BUF_SIZE = 512 };

void render_utils_generate_gpu_identifier(const char* vendor,
                                          const char* renderer, char* buffer,
                                          size_t size)
{
	if (!buffer || size == 0) {
		return;
	}

	const char* v_str = (vendor && vendor[0] != '\0') ? vendor : "unknown";
	const char* r_str =
	    (renderer && renderer[0] != '\0') ? renderer : "gpu";

	char raw[GPU_IDENTIFIER_RAW_BUF_SIZE];
	if (safe_snprintf(raw, GPU_IDENTIFIER_RAW_BUF_SIZE, "%s_%s", v_str,
	                  r_str) < 0) {
		(void)safe_snprintf(buffer, size, "%s", "unknown_gpu");
		return;
	}

	size_t dst_idx = 0;
	for (size_t i = 0; raw[i] != '\0'; i++) {
		append_sanitized_char(raw[i], buffer, &dst_idx, size);
	}

	// Trim trailing underscore
	if (dst_idx > 0 && buffer[dst_idx - 1] == '_') {
		dst_idx--;
	}
	buffer[dst_idx] = '\0';
}

void render_utils_get_gpu_identifier(char* buffer, size_t size)
{
	GPUInfo info = render_utils_get_gpu_info();
	render_utils_generate_gpu_identifier(info.vendor, info.renderer, buffer,
	                                     size);
}

GLuint render_utils_create_texture_2d(int width, int height,
                                      GLenum internal_format, GLint levels,
                                      const char* label)
{
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	if (label) {
		glObjectLabel(GL_TEXTURE, tex, -1, label);
	}

	glTexStorage2D(GL_TEXTURE_2D, levels, internal_format, width, height);

	/* Default parameters, caller can override if needed */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                (levels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

void render_utils_setup_sphere_instance_attributes(GLsizei stride,
                                                   size_t offset_albedo,
                                                   size_t offset_metallic)
{
	GLuint index_vattrib = 2; /* Start at 2 (0=Pos, 1=Norm usually) */

	/* mat4 model (Locations 2, 3, 4, 5) */
	for (int i = 0; i < 4; i++) {
		const void* m_offset =
		    utils_buffer_offset(i * 4 * sizeof(float));
		glEnableVertexAttribArray(index_vattrib);
		glVertexAttribPointer(index_vattrib, 4, GL_FLOAT, GL_FALSE,
		                      stride, m_offset);
		glVertexAttribDivisor(index_vattrib, 1);
		index_vattrib++;
	}

	/* Albedo (6) */
	glEnableVertexAttribArray(index_vattrib);
	const void* a_offset = utils_buffer_offset(offset_albedo);
	glVertexAttribPointer(index_vattrib, 3, GL_FLOAT, GL_FALSE, stride,
	                      a_offset);
	glVertexAttribDivisor(index_vattrib, 1);
	index_vattrib++;

	/* PBR (7) */
	glEnableVertexAttribArray(index_vattrib);
	glVertexAttribPointer(index_vattrib, 3, GL_FLOAT, GL_FALSE, stride,
	                      utils_buffer_offset(offset_metallic));
	glVertexAttribDivisor(index_vattrib, 1);
}

// -----------------------------------------------------------------------------
// State Management
// -----------------------------------------------------------------------------

GLStateBackup render_utils_save_state(void)
{
	GLStateBackup state;
	state.depth_enabled = glIsEnabled(GL_DEPTH_TEST);
	state.blend_enabled = glIsEnabled(GL_BLEND);
	glGetIntegerv(GL_POLYGON_MODE, state.polygon_mode);
	return state;
}

void render_utils_restore_state(const GLStateBackup* state)
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

void render_utils_setup_ui_state(void)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
