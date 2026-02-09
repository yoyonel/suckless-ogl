#include "texture.h"

#include "gl_common.h"
#include "log.h"
#include "render_utils.h"
#include "utils.h"
#include <math.h>
#include <stb_image.h>
#include <stddef.h>

enum { MAX_TEXTURE_DIMENSION = 8192 };

float* texture_load_pixels(const char* path, int* width, int* height,
                           int* channels)
{
	CLEANUP_FILE FILE* file_ptr = fopen(path, "rb");
	if (!file_ptr) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to open HDR image: %s", path);
		return NULL;
	}

	int img_width = 0;
	int img_height = 0;
	int comp = 0;
	if (!stbi_info_from_file(file_ptr, &img_width, &img_height, &comp)) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to read HDR image header: %s", path);
		return NULL;
	}

	if (img_width > MAX_TEXTURE_DIMENSION ||
	    img_height > MAX_TEXTURE_DIMENSION) {
		LOG_ERROR("suckless-ogl.texture",
		          "HDR image exceeds max dimensions: %s (%dx%d > %d)",
		          path, img_width, img_height, MAX_TEXTURE_DIMENSION);
		return NULL;
	}

	if (fseek(file_ptr, 0, SEEK_SET) != 0) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to rewind HDR image file: %s", path);
		return NULL;
	}

	float* data =
	    stbi_loadf_from_file(file_ptr, width, height, channels, 4);
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

	if (width > MAX_TEXTURE_DIMENSION || height > MAX_TEXTURE_DIMENSION) {
		LOG_ERROR("suckless-ogl.texture",
		          "Texture exceeds max dimensions: %dx%d > %d", width,
		          height, MAX_TEXTURE_DIMENSION);
		return 0;
	}

	/* Clear any previous sticky errors to ensure accurate results */
	(void)glGetError();

	int levels = 1;
	if (width > 0 || height > 0) {
		levels =
		    (int)floor(log2(fmax((double)width, (double)height))) + 1;
	}

	// NOLINTNEXTLINE(misc-include-cleaner)
	GLuint CLEANUP_TEXTURE tex = render_utils_create_texture_2d(
	    width, height, GL_RGBA16F, levels, "HDR Texture");

	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
	                GL_FLOAT, data);

	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		LOG_ERROR("suckless-ogl.texture",
		          "GL error after glTexSubImage2D: 0x%x", err);
		return 0;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); /* Restore default */

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
	CLEANUP_FILE FILE* file_ptr = fopen(path, "rb");
	if (!file_ptr) {
		LOG_ERROR("suckless-ogl.texture", "Failed to open image: %s",
		          path);
		return 0;
	}

	int width = 0;
	int height = 0;
	int channels = 0;

	if (!stbi_info_from_file(file_ptr, &width, &height, &channels)) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to read image header: %s", path);
		return 0;
	}

	if (width > MAX_TEXTURE_DIMENSION || height > MAX_TEXTURE_DIMENSION) {
		LOG_ERROR("suckless-ogl.texture",
		          "Image exceeds max dimensions: %s (%dx%d > %d)", path,
		          width, height, MAX_TEXTURE_DIMENSION);
		return 0;
	}

	if (fseek(file_ptr, 0, SEEK_SET) != 0) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to rewind image file: %s", path);
		return 0;
	}

	/* Force 4 channels (RGBA) */
	unsigned char* data =
	    stbi_load_from_file(file_ptr, &width, &height, &channels, 4);
	if (!data) {
		LOG_ERROR("suckless-ogl.texture", "Failed to load image: %s",
		          path);
		return 0;
	}

	int levels = 1;
	if (width > 0 || height > 0) {
		levels =
		    (int)floor(log2(fmax((double)width, (double)height))) + 1;
	}

	// NOLINTNEXTLINE(misc-include-cleaner)
	GLuint CLEANUP_TEXTURE tex = render_utils_create_texture_2d(
	    width, height, GL_RGBA8, levels, path);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
	                GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(data);

	LOG_INFO("suckless-ogl.texture", "Loaded texture: %s (%dx%d)", path,
	         width, height);

	return TRANSFER_OWNERSHIP(tex);
}
