/**
 * @file skybox.h
 * @brief Infinite-distance background rendering (fullscreen quad approach).
 *
 * This module renders the environment map using a raycasting approach on a
 * full-screen quad, which is more efficient than a traditional cube mesh
 * and easier to integrate with depth-based post-processing.
 */

#ifndef SKYBOX_H
#define SKYBOX_H

#include "gl_common.h"
#include <cglm/types.h>

typedef struct Shader Shader;

/**
 * @struct Skybox
 * @brief Persistent state and resource handles for the environment renderer.
 */
typedef struct {
	GLuint vao; /**< Shared full-screen quad VAO. */
	GLuint vbo; /**< Quad vertex buffer. */

	/* Cached uniform locations */
	GLint u_inv_view_proj; /**< Uniform location for the inverse View-Proj
	                          matrix. */
	GLint u_blur_lod; /**< Uniform location for environment blur (lod). */
	GLint u_env_map;  /**< Uniform location for the HDR cubemap sampler. */
} Skybox;

/**
 * @brief Initializes skybox geometry and caches uniform locations.
 * @param skybox Pointer to the struct.
 * @param shader The compiled skybox shader wrapper.
 */
void skybox_init(Skybox* skybox, Shader* shader);

/**
 * @brief Renders the skybox to the current framebuffer.
 * @param skybox Pointer to the struct.
 * @param shader Shader to use.
 * @param env_map Primary HDR cubemap texture.
 * @param fallback_tex Dummy texture if env_map is invalid.
 * @param inv_view_proj Inverse of the current View-Proj matrix.
 * @param blur_lod Level of mip-map detail (0.0 for sharp, higher for blurry).
 */
void skybox_render(Skybox* skybox, Shader* shader, GLuint env_map,
                   GLuint fallback_tex, const mat4 inv_view_proj,
                   float blur_lod);

/**
 * @brief Cleans up GPU resources.
 * @param skybox Pointer to the struct.
 */
void skybox_cleanup(Skybox* skybox);

#endif /* SKYBOX_H */
