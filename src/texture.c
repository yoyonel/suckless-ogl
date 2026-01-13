#include "texture.h"

#include <stdio.h>
#include <stdlib.h>
#include "log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

GLuint texture_load_hdr(const char* path, int* width, int* height)
{
	int channels;
	float* data = stbi_loadf(path, width, height, &channels, 0);
	if (!data) {
		LOG_ERROR("suckless-ogl.texture", "Failed to load HDR image: %s", path);
		return 0;
	}

	LOG_INFO("suckless-ogl.texture", "HDR image: %dx%d, channels=%d", *width, *height, channels);

	/* Create OpenGL texture */
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, *width, *height, 0, GL_RGB,
	             GL_FLOAT, data);

	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		LOG_ERROR("suckless-ogl.texture", "GL error after texture upload: 0x%x", err);
		stbi_image_free(data);
		glDeleteTextures(1, &tex);
		return 0;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(data);

	return tex;
}


