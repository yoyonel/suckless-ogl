#ifndef FX_UTILS_H
#define FX_UTILS_H

#include "gl_common.h"

/**
 * @brief Configuration parameters for creating an effect texture.
 */
typedef struct {
	int width;
	int height;
	GLenum internal_format;
	GLenum format;
	GLenum type;
	GLint min_filter;
	GLint mag_filter;
	GLint wrap_s;
	GLint wrap_t;
	const void* initial_data; /* Can be NULL */
} FXTextureConfig;

/**
 * @brief Creates a texture, allocates memory, and sets filtering/wrapping
 * parameters.
 * @param tex Pointer to the GLuint where the texture ID will be stored.
 *        If *tex is not 0, it deletes the old texture first.
 * @param config The texture configuration parameters.
 */
void fx_utils_create_texture(GLuint* tex, const FXTextureConfig* config);

#endif /* FX_UTILS_H */
