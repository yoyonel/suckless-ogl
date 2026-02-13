#include "texture.h"

#include "gl_common.h"
#include "log.h"
#include "utils.h"
#include <math.h>
#include <stb_image.h>
#include <stddef.h>
#include <stdio.h>

#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"
#endif

float* texture_load_pixels(const char* path, int* width, int* height,
                           int* channels)
{
#ifdef TRACY_ENABLE
	TracyCZoneNC(io_ctx, "Disk Read", 0x3498db, 1);
#endif
	size_t file_size = 0;
	void* file_data = 0;
	FILE* file = fopen(path, "rb");
	if (!file) {
		LOG_ERROR("suckless-ogl.texture", "Failed to open image: %s",
		          path);
#ifdef TRACY_ENABLE
		TracyCZoneEnd(io_ctx);
#endif
		return NULL;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to seek end of image: %s", path);
		fclose(file);
#ifdef TRACY_ENABLE
		TracyCZoneEnd(io_ctx);
#endif
		return NULL;
	}

	long ftell_pos = ftell(file);
	if (ftell_pos < 0) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to tell image size: %s", path);
		fclose(file);
#ifdef TRACY_ENABLE
		TracyCZoneEnd(io_ctx);
#endif
		return NULL;
	}
	file_size = (size_t)ftell_pos;

	if (fseek(file, 0, SEEK_SET) != 0) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to reset image cursor: %s", path);
		fclose(file);
#ifdef TRACY_ENABLE
		TracyCZoneEnd(io_ctx);
#endif
		return NULL;
	}

	file_data = malloc(file_size);
	if (!file_data) {
		fclose(file);
#ifdef TRACY_ENABLE
		TracyCZoneEnd(io_ctx);
#endif
		return NULL;
	}

	if (fread(file_data, 1, file_size, file) != file_size) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to read image data: %s", path);
		free(file_data);
		fclose(file);
#ifdef TRACY_ENABLE
		TracyCZoneEnd(io_ctx);
#endif
		return NULL;
	}

	if (fclose(file) != 0) {
		LOG_WARNING("suckless-ogl.texture",
		            "Failed to close image file: %s", path);
	}

#ifdef TRACY_ENABLE
	TracyCZoneEnd(io_ctx);
#endif

	int img_width = 0;
	int img_height = 0;
	int img_channels = 0;

#ifdef TRACY_ENABLE
	TracyCZoneNC(ctx_info, "STBI Info", 0x2ecc71, 1);
#endif
	if (!stbi_info_from_memory(file_data, (int)file_size, &img_width,
	                           &img_height, &img_channels)) {
		free(file_data);
#ifdef TRACY_ENABLE
		TracyCZoneEnd(ctx_info);
#endif
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to parse image info: %s", path);
		return NULL;
	}
#ifdef TRACY_ENABLE
	TracyCZoneEnd(ctx_info);
#endif

	if (img_width > MAX_TEXTURE_DIMENSION ||
	    img_height > MAX_TEXTURE_DIMENSION) {
		free(file_data);
		LOG_ERROR("suckless-ogl.texture",
		          "Image exceeds max dimensions: %s (%dx%d > %d)", path,
		          img_width, img_height, MAX_TEXTURE_DIMENSION);
		return NULL;
	}

#ifdef TRACY_ENABLE
	TracyCZoneNC(ctx_load, "STBI Decode HDR", 0xe67e22, 1);
	TracyCZoneText(ctx_load, path, strlen(path));
#endif
	float* data = stbi_loadf_from_memory(file_data, (int)file_size, width,
	                                     height, channels, 4);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(ctx_load);
#endif
	free(file_data);

	if (!data) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to decode HDR pixels: %s", path);
		return NULL;
	}

	/* Double-check dimensions after full load */
	if (*width > MAX_TEXTURE_DIMENSION || *height > MAX_TEXTURE_DIMENSION) {
		LOG_ERROR("suckless-ogl.texture",
		          "Image exceeds max dimensions after decode: %s",
		          path);
		stbi_image_free(data);
		return NULL;
	}

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
