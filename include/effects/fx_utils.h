#ifndef FX_UTILS_H
#define FX_UTILS_H

#include "gl_common.h"

/**
 * @brief Configuration parameters for creating an effect texture.
 */
typedef struct {
	int width;  /**< Texture width in pixels. */
	int height; /**< Texture height in pixels. */
	GLenum
	    internal_format; /**< OpenGL internal format (e.g., GL_RGBA16F). */
	GLenum format;       /**< OpenGL data format (e.g., GL_RGBA). */
	GLenum type;         /**< OpenGL data type (e.g., GL_FLOAT). */
	GLint min_filter;    /**< Minification filter (e.g., GL_LINEAR). */
	GLint mag_filter;    /**< Magnification filter (e.g., GL_LINEAR). */
	GLint wrap_s; /**< S-coordinate wrap mode (e.g., GL_CLAMP_TO_EDGE). Set
	                 to 0 or FX_TEXTURE_WRAP_NONE to skip. */
	GLint wrap_t; /**< T-coordinate wrap mode (e.g., GL_CLAMP_TO_EDGE). Set
	                 to 0 or FX_TEXTURE_WRAP_NONE to skip. */
	const void* initial_data; /**< Initial texture data. Can be NULL. */
} FXTextureConfig;

/**
 * @brief Sentinel value to skip setting a wrap parameter.
 */
#define FX_TEXTURE_WRAP_NONE 0

/**
 * @brief Creates a texture, allocates memory, and sets filtering/wrapping
 * parameters.
 * @param tex Pointer to the GLuint where the texture ID will be stored.
 *        If *tex is not 0, it deletes the old texture first.
 * @param config The texture configuration parameters.
 */
void fx_utils_create_texture(GLuint* tex, const FXTextureConfig* config);

#endif /* FX_UTILS_H */
