/**
 * @file texture.h
 * @brief Image loading and GPU texture management.
 */

#ifndef TEXTURE_H
#define TEXTURE_H

#include "gl_common.h"

enum { MAX_TEXTURE_DIMENSION = 8192 };

/**
 * @brief Loads an HDR (radiance) image and uploads it to the GPU.
 * @param path File system path.
 * @param[out] width Pointer to store image width.
 * @param[out] height Pointer to store image height.
 * @return GLuint handle of the allocated HDR texture.
 */
GLuint texture_load_hdr(const char* path, int* width, int* height);

/**
 * @brief Uploads raw floating-point RGB data to an OpenGL texture.
 * @param data Array of floats (RGB).
 * @param width Texture width.
 * @param height Texture height.
 * @return GLuint handle.
 */
GLuint texture_upload_hdr(float* data, int width, int height);

/**
 * @brief Loads a standard 8-bit image (PNG, JPG) and uploads it to the GPU.
 * @param path File system path.
 * @return GLuint handle.
 */
GLuint texture_load(const char* path);

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

#endif /* TEXTURE_H */
