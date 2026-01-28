#include "texture.h"

#include "gl_common.h"
#include "log.h"
#include "utils.h"
#include <math.h>
#include <stb_image.h>
#include <stddef.h>

float* texture_load_pixels(const char* path, int* width, int* height,
                           int* channels)
{
	float* data = stbi_loadf(path, width, height, channels, 4);
	if (!data) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to load HDR image: %s", path);
		return NULL;
	}
	LOG_INFO("suckless-ogl.texture",
	         "HDR image loaded (CPU): %dx%d, channels=%d", *width, *height,
	         *channels);
	return data;
}

GLuint texture_upload_hdr(float* data, int width, int height)
{
	if (!data) {
		return 0;
	}

	/* Clear any previous sticky errors to ensure accurate results */
	(void)glGetError();

	GLuint CLEANUP_TEXTURE tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	/* Safer levels calculation to avoid edge cases */
	int levels = 1;
	if (width > 0 || height > 0) {
		levels =
		    (int)floor(log2(fmax((double)width, (double)height))) + 1;
	}

	glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA16F, width, height);

	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		LOG_ERROR("suckless-ogl.texture",
		          "GL error after glTexStorage2D: 0x%x (levels: %d, "
		          "size: %dx%d)",
		          err, levels, width, height);
		return 0;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
	                GL_FLOAT, data);

	err = glGetError();
	if (err != GL_NO_ERROR) {
		LOG_ERROR("suckless-ogl.texture",
		          "GL error after glTexSubImage2D: 0x%x", err);
		return 0;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); /* Restore default */

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenerateMipmap(GL_TEXTURE_2D);

	err = glGetError();
	if (err != GL_NO_ERROR) {
		LOG_ERROR("suckless-ogl.texture",
		          "GL error after mipmap/params: 0x%x", err);
		return 0;
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	return TRANSFER_OWNERSHIP(tex);
}

GLuint texture_load_hdr(const char* path, int* width, int* height)
{
	int channels = 0;
	CLEANUP_FREE float* data =
	    texture_load_pixels(path, width, height, &channels);
	if (!data) {
		return 0;
	}

	return texture_upload_hdr(data, *width, *height);
}

GLuint texture_load(const char* path)
{
	int width = 0;
	int height = 0;
	int channels = 0;

	/* Force 4 channels (RGBA) */
	unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
	if (!data) {
		LOG_ERROR("suckless-ogl.texture", "Failed to load image: %s",
		          path);
		return 0;
	}

	GLuint CLEANUP_TEXTURE tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
	             GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(data);

	LOG_INFO("suckless-ogl.texture", "Loaded texture: %s (%dx%d)", path,
	         width, height);

	return TRANSFER_OWNERSHIP(tex);
}
