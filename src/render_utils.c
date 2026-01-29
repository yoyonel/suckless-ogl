#include "render_utils.h"

#include "gl_common.h"
#include "log.h"
#include <stddef.h>

// -----------------------------------------------------------------------------
// Texture Management
// -----------------------------------------------------------------------------

GLuint render_utils_create_color_texture(float red, float green, float blue,
                                         float alpha)
{
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	float color[4] = {red, green, blue, alpha};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_FLOAT,
	             color);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Setup debug label based on color
	if (red == 0 && green == 0 && blue == 0) {
		glObjectLabel(GL_TEXTURE, tex, -1, "Dummy Black");
	} else if (red == 1 && green == 1 && blue == 1) {
		glObjectLabel(GL_TEXTURE, tex, -1, "Dummy White");
	} else {
		glObjectLabel(GL_TEXTURE, tex, -1, "Dummy Color");
	}

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

void render_utils_create_fullscreen_quad(GLuint* vao, GLuint* vbo)
{
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
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
	                      BUFFER_OFFSET(2 * sizeof(float)));
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
