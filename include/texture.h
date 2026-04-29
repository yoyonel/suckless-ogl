/**
 * @file texture.h
 * @brief Image loading and GPU texture management.
 */

#ifndef TEXTURE_H
#define TEXTURE_H

#include "gl_common.h"

enum { MAX_TEXTURE_DIMENSION = 8192 };

/**
 * @brief Ensures a PBO is allocated with sufficient size.
 *
 * Checks if internal *current_size is >= required_size. If not, reallocates
 * (orphans) the buffer using glBufferData.
 *
 * @param pbo_id Pointer to the PBO ID (generated if 0).
 * @param current_size Pointer to the currently tracking size (updated on
 * realloc).
 * @param required_size The minimum size needed.
 */
void texture_ensure_pbo(GLuint* pbo_id, GLsizeiptr* current_size,
                        GLsizeiptr required_size);

/**
 * @brief Maps a PBO for writing.
 *
 * @param pbo_id ID of the PBO to map.
 * @param size_bytes Size to map (should match creation size).
 * @return Pointer to mapped memory, or NULL on failure.
 */
void* texture_map_pbo(GLuint pbo_id, size_t size_bytes);

/**
 * @brief Pre-allocates VRAM for an HDR texture (glTexStorage2D only).
 *
 * Call this early (e.g. when async loader requests a PBO) to spread the
 * cost of texture allocation across frames. If old_tex already matches
 * the requested dimensions, it is returned as-is (zero-cost reuse).
 *
 * @param width Texture width.
 * @param height Texture height.
 * @param old_tex Existing texture ID to check for reuse (0 if none).
 * @return GLuint Pre-allocated texture ID, or 0 on failure.
 */
GLuint texture_preallocate_hdr(int width, int height, GLuint old_tex);

/**
 * @brief Finalizes HDR upload from a PBO (Unmap -> Upload -> Mipmaps).
 *
 * @param pbo_id ID of the PBO (must be bound).
 * @param ptr Pointer to the mapped PBO memory (will be unmapped).
 * @param width Texture width.
 * @param height Texture height.
 * @param reuse_tex_id Existing texture ID to update/reuse.
 * @return GLuint The texture ID (new or reused).
 */
GLuint texture_upload_hdr_from_pbo(GLuint pbo_id, void* ptr, int width,
                                   int height, GLuint reuse_tex_id);

/**
 * @brief Generates mipmaps for an HDR texture.
 *
 * @param tex The OpenGL ID of the texture.
 */
void texture_generate_hdr_mipmap(GLuint tex);

/**
 * @brief Decodes an HDR image into RAM without uploading to GPU.
 *
 * Useful for asynchronous loading or CPU-side processing.
 * @param path File system path.
 * @param[out] width Width.
 * @param[out] height Height.
 * @param[out] channels Number of color channels.
 * @return Pointer to heap-allocated data. Buffer must be freed by caller.
 */
float* texture_load_pixels(const char* path, int* width, int* height,
                           int* channels);

/**
 * @brief Loads a PNG image from disk and creates an OpenGL RGBA texture.
 *
 * @param path File system path to the PNG image.
 * @return GLuint The OpenGL texture ID, or 0 on failure.
 */
GLuint texture_load_rgba_png(const char* path);

#endif /* TEXTURE_H */
