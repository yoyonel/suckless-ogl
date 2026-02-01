/**
 * @file ssbo_rendering.h
 * @brief Buffer-backed instancing for ultra-large-scale rendering.
 *
 * This module uses Shader Storage Buffer Objects (SSBO) to pass instance
 * data to the shaders. SSBOs allow for much larger data sets than standard
 * uniform buffers or vertex attributes on some hardware.
 */

#ifndef SSBO_RENDERING_H
#define SSBO_RENDERING_H

#include "gl_common.h"
#include <cglm/types.h>

/**
 * @struct SphereInstanceSSBO
 * @brief Aligned structure for SSBO storage (std430 layout).
 *
 * Each instance occupies exactly 80 bytes to maintain alignment.
 */
typedef struct {
	mat4 model;        /**< 64 bytes (4x vec4). */
	vec3 albedo;       /**< 12 bytes. */
	float metallic;    /**< 4 bytes. */
	float roughness;   /**< 4 bytes. */
	float ao;          /**< 4 bytes. */
	float _padding[2]; /**< 8 bytes - padding to reach 80 bytes total. */
} SphereInstanceSSBO;

/**
 * @struct SSBOGroup
 * @brief GPU resources for an SSBO-based render group.
 */
typedef struct {
	GLuint ssbo;        /**< The Shader Storage Buffer handle. */
	GLuint vao;         /**< Vertex Array Object for drawing. */
	int instance_count; /**< Number of spheres in this group. */
} SSBOGroup;

/**
 * @brief Initializes an SSBO group with instance data.
 * @param group Pointer to the struct.
 * @param data Array of instance data.
 * @param count Number of instances.
 */
void ssbo_group_init(SSBOGroup* group, const SphereInstanceSSBO* data,
                     int count);

/**
 * @brief Binds a mesh (VBO/EBO) to the SSBO VAO.
 * @param group Pointer to the struct.
 * @param vbo Vertex buffer.
 * @param nbo Normal buffer.
 * @param ebo Element (index) buffer.
 */
void ssbo_group_bind_mesh(SSBOGroup* group, GLuint vbo, GLuint nbo, GLuint ebo);

/**
 * @brief Renders the group using instanced drawing.
 * @param group Pointer to the struct.
 * @param index_count Number of indices per sphere mesh.
 */
void ssbo_group_draw(SSBOGroup* group, size_t index_count);

/**
 * @brief Releases GPU resources.
 * @param group Pointer to the struct.
 */
void ssbo_group_cleanup(SSBOGroup* group);

#endif /* SSBO_RENDERING_H */
