#include "texture.h"

#include "gl_common.h"
#include "io.h"
#include "log.h"
#include "profiler.h"
#include "utils.h"
#include <math.h>
#include <stb_image.h>
#include <stddef.h>
#include <stdio.h>

static const uint32_t TRACY_COLOR_TEXTURE_UPLOAD = 0xAAAA55;
static const uint32_t TRACY_COLOR_TEXTURE_STORAGE = 0xAA55AA;
static const uint32_t TRACY_COLOR_MIPMAP_GEN = 0x55AAAA;
static const uint32_t TRACY_COLOR_TEXTURE_UPLOAD_FULL = 0xFF8800;

enum { MAX_TEXTURE_FILE_SIZE = 64 * 1024 * 1024 };

float* texture_load_pixels(const char* path, int* width, int* height,
                           int* channels)
{
	size_t content_size = 0;
	/* Limit to 64MB for textures */
	CLEANUP_FREE unsigned char* content = (unsigned char*)io_read_file(
	    path, MAX_TEXTURE_FILE_SIZE, &content_size);
	if (!content) {
		return NULL;
	}

	int img_width = 0;
	int img_height = 0;
	int img_channels = 0;
	if (!stbi_info_from_memory(content, (int)content_size, &img_width,
	                           &img_height, &img_channels)) {
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

	float* data = stbi_loadf_from_memory(content, (int)content_size, width,
	                                     height, channels, 4);
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

	return data;
}

void texture_ensure_pbo(GLuint* pbo_id, GLsizeiptr* current_size,
                        GLsizeiptr required_size)
{
	if (!pbo_id) {
		return;
	}

	if (*pbo_id == 0) {
		glGenBuffers(1, pbo_id);
		*current_size = 0;
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo_id);

	if (*current_size < required_size) {
		/* Allocate only if current size is insufficient.
		 * We avoid repetitive glBufferData calls (orphaning) to prevent
		 * allocation overhead, relying on double-buffering and
		 * unsynchronized mapping to handle synchronization.
		 */
		glBufferData(GL_PIXEL_UNPACK_BUFFER, required_size, NULL,
		             GL_STREAM_DRAW);
		*current_size = required_size;
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void* texture_map_pbo(GLuint pbo_id, size_t size_bytes)
{
	if (pbo_id == 0) {
		return NULL;
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_id);

	/* Map for writing, invalidate previous content to avoid stalls
	 * We use INVALIDATE_BUFFER_BIT to allow the driver to orphan the buffer
	 * if it's still in use, without needing explicit glBufferData(NULL).
	 */
	/* Map for writing.
	 * We use GL_MAP_UNSYNCHRONIZED_BIT because we manually manage
	 * synchronization via double-buffering. We don't want the driver
	 * to wait for previous operations on this buffer, as we guarantee
	 * (via ping-pong) that it's safe to use.
	 * We also use INVALIDATE_BUFFER_BIT to hint that we will overwrite
	 * the entire buffer content.
	 */
	GLbitfield access = (GLbitfield)GL_MAP_WRITE_BIT |
	                    (GLbitfield)GL_MAP_INVALIDATE_BUFFER_BIT |
	                    (GLbitfield)GL_MAP_UNSYNCHRONIZED_BIT;

	void* ptr = NULL;
	ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
	                       (GLsizeiptr)size_bytes, access);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	return ptr;
}

/**
 * @brief Check if an existing texture matches the expected HDR format.
 *
 * Queries GL state for the texture's level-0 dimensions and internal format.
 * The texture must already be bound to GL_TEXTURE_2D before calling.
 *
 * @return true if width, height and format (GL_RGBA16F) all match.
 */
static bool texture_matches_hdr(int width, int height)
{
	int existing_w = 0;
	int existing_h = 0;
	int existing_fmt = 0;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,
	                         &existing_w);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
	                         &existing_h);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT,
	                         &existing_fmt);
	return existing_w == width && existing_h == height &&
	       existing_fmt == GL_RGBA16F;
}

GLuint texture_preallocate_hdr(int width, int height, GLuint old_tex)
{
	TRACE_GPU_SCOPE("TexPreallocHDR", TRACY_COLOR_TEXTURE_STORAGE);

	if (width > MAX_TEXTURE_DIMENSION || height > MAX_TEXTURE_DIMENSION) {
		return 0;
	}

	/* If old_tex already matches, keep it — zero-cost reuse */
	if (old_tex != 0) {
		glBindTexture(GL_TEXTURE_2D, old_tex);
		if (texture_matches_hdr(width, height)) {
			glBindTexture(GL_TEXTURE_2D, 0);
			LOG_INFO("suckless-ogl.texture",
			         "Pre-alloc: reusing texture %u (%dx%d)",
			         old_tex, width, height);
			return old_tex;
		}
		/* Dimensions/format mismatch — delete and re-allocate */
		glDeleteTextures(1, &old_tex);
	}

	/* Allocate base level only (mutable texture).
	 * glTexImage2D(level 0, NULL) allocates just ~64MB for the base
	 * level instead of ~85MB for 13 mip levels with glTexStorage2D.
	 * The mipmap chain will be created later by glGenerateMipmap()
	 * during the actual upload frame.
	 *
	 * No glGetError() here to avoid forcing a GPU sync point —
	 * errors will be caught during the upload phase.
	 */
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
	             GL_HALF_FLOAT, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);
	LOG_INFO("suckless-ogl.texture",
	         "Pre-allocated HDR texture %u (%dx%d, base level only)", tex,
	         width, height);
	return tex;
}

GLuint texture_upload_hdr_from_pbo(GLuint pbo_id, int width, int height,
                                   GLuint reuse_tex_id)
{
	TRACE_GPU_SCOPE("TextureUploadHDR_PBO",
	                TRACY_COLOR_TEXTURE_UPLOAD_FULL);

	if (width > MAX_TEXTURE_DIMENSION || height > MAX_TEXTURE_DIMENSION) {
		LOG_ERROR("suckless-ogl.texture",
		          "Texture exceeds max dimensions: %dx%d > %d", width,
		          height, MAX_TEXTURE_DIMENSION);
		return 0;
	}

	GLuint CLEANUP_TEXTURE tex = 0;
	bool is_reused = false;

	/* Calculate levels */
	int levels = 1;
	if (width > 0 || height > 0) {
		levels =
		    (int)floor(log2(fmax((double)width, (double)height))) + 1;
	}

	/* Attempt to reuse existing texture */
	if (reuse_tex_id != 0) {
		glBindTexture(GL_TEXTURE_2D, reuse_tex_id);
		if (texture_matches_hdr(width, height)) {
			tex = reuse_tex_id;
			is_reused = true;
			LOG_INFO("suckless-ogl.texture",
			         "Reusing cached HDR texture ID %u (%dx%d)",
			         tex, width, height);
		} else {
			LOG_INFO("suckless-ogl.texture",
			         "Recycled texture ID %u mismatch. Deleting.",
			         reuse_tex_id);
			glDeleteTextures(1, &reuse_tex_id);
		}
	}

	if (!is_reused) {
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		{
			TRACE_GPU_SCOPE("TexStorageHDR",
			                TRACY_COLOR_TEXTURE_STORAGE);
			glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA16F, width,
			               height);
		}

#ifndef NDEBUG
		GLenum err = glGetError();
		if (err != GL_NO_ERROR) {
			LOG_ERROR("suckless-ogl.texture",
			          "GL error after glTexStorage2D: 0x%x", err);
			return 0;
		}
#endif
	} else {
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_id);
	if (!glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER)) {
		LOG_ERROR("suckless-ogl.texture",
		          "Failed to unmap PBO %u! Data might be corrupted.",
		          pbo_id);
		/* Continue anyway? Or return 0? safest is to return 0 or try to
		 * re-upload? For now just log error. */
	}

	{
		TRACE_GPU_SCOPE("TexUploadHDR_SubImage",
		                TRACY_COLOR_TEXTURE_UPLOAD);
		/* Offset 0 in PBO */
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
		                GL_HALF_FLOAT, 0);
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
	return TRANSFER_OWNERSHIP(tex);
}

void texture_generate_hdr_mipmap(GLuint tex)
{
	if (tex == 0) {
		return;
	}
	TRACE_GPU_SCOPE("GenMipmapHDR", TRACY_COLOR_MIPMAP_GEN);
	glBindTexture(GL_TEXTURE_2D, tex);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
}
