#include "texture.h"

#include "gl_common.h"
#include "log.h"
#include "utils.h"
#include <math.h>
#include <stb_image.h>
#include <stddef.h>
#include <stdio.h>

float* texture_load_pixels(const char* path, int* width, int* height,
                           int* channels)
{
	if (!is_safe_path(path)) {
		LOG_ERROR("suckless-ogl.texture",
		          "Security Violation: Unsafe path detected: %s", path);
		return NULL;
	}

	CLEANUP_FILE FILE* file = fopen(path, "rb");
	if (!file) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to open HDR image: %s", path);
		return NULL;
	}

	int img_width = 0;
	int img_height = 0;
	int img_channels = 0;
	if (!stbi_info_from_file(file, &img_width, &img_height,
	                         &img_channels)) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to parse HDR image info: %s", path);
		return NULL;
	}

	if (img_width > MAX_TEXTURE_DIMENSION ||
	    img_height > MAX_TEXTURE_DIMENSION) {
		LOG_ERROR("suckless-ogl.texture",
		          "HDR image exceeds max dimensions: %s (%dx%d > %d)",
		          path, img_width, img_height, MAX_TEXTURE_DIMENSION);
		return NULL;
	}

	if (fseek(file, 0, SEEK_SET) != 0) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to reset file cursor: %s", path);
		return NULL;
	}

	float* data = stbi_loadf_from_file(file, width, height, channels, 4);
	if (!data) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to load HDR pixels: %s", path);
		return NULL;
	}

	/* Double-check dimensions after full load to prevent TOCTOU attacks */
	if (*width > MAX_TEXTURE_DIMENSION || *height > MAX_TEXTURE_DIMENSION) {
		LOG_ERROR(
		    "suckless-ogl.texture",
		    "HDR image exceeds max dimensions after load: %s (%dx%d "
		    "> %d)",
		    path, *width, *height, MAX_TEXTURE_DIMENSION);
		stbi_image_free(data);
		return NULL;
	}

	LOG_INFO("suckless-ogl.texture",
	         "HDR image loaded (CPU): %dx%d, channels=%d", *width, *height,
	         *channels);

	/* File is automatically closed by CLEANUP_FILE */
	return data;
}

GLuint texture_upload_hdr(float* data, int width, int height)
{
	if (!data) {
		return 0;
	}

	if (width > MAX_TEXTURE_DIMENSION || height > MAX_TEXTURE_DIMENSION) {
		LOG_ERROR("suckless-ogl.texture",
		          "Texture exceeds max dimensions: %dx%d > %d", width,
		          height, MAX_TEXTURE_DIMENSION);
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
