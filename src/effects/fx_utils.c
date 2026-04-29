#include "effects/fx_utils.h"

#include "gl_common.h"
#include "log.h"

void fx_utils_create_texture(GLuint* tex, const FXTextureConfig* config)
{
	if (!config || !tex) {
		LOG_ERROR("suckless-ogl.effects.utils",
		          "Invalid parameters for texture creation");
		return;
	}

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

	if (config->wrap_s != FX_TEXTURE_WRAP_NONE) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
		                config->wrap_s);
	}
	if (config->wrap_t != FX_TEXTURE_WRAP_NONE) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
		                config->wrap_t);
	}
}
