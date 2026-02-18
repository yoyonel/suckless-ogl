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
// Debugging / Validation
// -----------------------------------------------------------------------------

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
		buffer[(*dst_idx)++] = (char)tolower(unsigned_char);
		return;
	}

	// Handle separators: convert to underscore, but avoid leading or
	// consecutive underscores
	bool is_sep = (unsigned_char == ' ' || unsigned_char == '_' ||
	               unsigned_char == '-' || unsigned_char == '.');
	if (is_sep && *dst_idx > 0 && buffer[*dst_idx - 1] != '_') {
		buffer[(*dst_idx)++] = '_';
	}
}

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

	static const size_t RAW_BUF_SIZE = 512;
	char raw[RAW_BUF_SIZE];
	if (!safe_snprintf(raw, RAW_BUF_SIZE, "%s_%s", v_str, r_str)) {
		safe_snprintf(buffer, size, "%s", "unknown_gpu");
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

void render_utils_setup_sphere_instance_attributes(GLsizei stride,
                                                   size_t offset_albedo,
                                                   size_t offset_metallic)
{
	GLuint index_vattrib = 2; /* Start at 2 (0=Pos, 1=Norm usually) */

	/* mat4 model (Locations 2, 3, 4, 5) */
	for (int i = 0; i < 4; i++) {
		glEnableVertexAttribArray(index_vattrib);
		glVertexAttribPointer(index_vattrib, 4, GL_FLOAT, GL_FALSE,
		                      stride,
		                      // NOLINTNEXTLINE(misc-include-cleaner)
		                      BUFFER_OFFSET(i * 4 * sizeof(float)));
		glVertexAttribDivisor(index_vattrib, 1);
		index_vattrib++;
	}

	/* Albedo (6) */
	glEnableVertexAttribArray(index_vattrib);
	glVertexAttribPointer(index_vattrib, 3, GL_FLOAT, GL_FALSE, stride,
	                      BUFFER_OFFSET(offset_albedo));
	glVertexAttribDivisor(index_vattrib, 1);
	index_vattrib++;

	/* PBR (7) */
	glEnableVertexAttribArray(index_vattrib);
	glVertexAttribPointer(index_vattrib, 3, GL_FLOAT, GL_FALSE, stride,
	                      BUFFER_OFFSET(offset_metallic));
	glVertexAttribDivisor(index_vattrib, 1);
}
