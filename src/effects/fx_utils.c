#include "effects/fx_utils.h"

#include "gl_common.h"
#include <stddef.h>

void fx_utils_create_texture(GLuint* tex, const FXTextureConfig* config)
{
	if (*tex) {
		glDeleteTextures(1, tex);
	}

	glGenTextures(1, tex);
	glBindTexture(GL_TEXTURE_2D, *tex);

	glTexImage2D(GL_TEXTURE_2D, 0, (GLint)config->internal_format,
	             config->width, config->height, 0, config->format,
	             config->type, config->initial_data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                config->min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
	                config->mag_filter);

	if (config->wrap_s != 0) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
		                config->wrap_s);
	}
	if (config->wrap_t != 0) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
		                config->wrap_t);
	}
}
