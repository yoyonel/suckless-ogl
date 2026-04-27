/**
 * @file instanced_rendering.h
 * @brief Mesh-based instanced rendering for spheres.
 *
 * This module handles high-performance drawing of multiple mesh-based spheres
 * using `glDrawElementsInstanced`. It uses a SIMD-aligned structure for
 * instance data.
 */

#ifndef INSTANCED_RENDERING_H
#define INSTANCED_RENDERING_H

#include "gl_common.h"
#include "sphere_types.h"

/**
 * @struct InstancedGroup
 * @brief Manages mesh-based instanced spheres on the GPU.
 */
typedef struct {
	GLuint vao; /**< Dedicated VAO linking mesh attributes and instance
	               data. */
	GLuint instance_vbo; /**< GPU buffer storing instances on GPU. */
	int instance_count;  /**< Number of spheres in this group. */
} InstancedGroup;

/**
 * @brief Allocates the instance VBO on the GPU.
 * @param group Pointer to the group.
 * @param data Initial data (optional).
 * @param count Number of instances.
 */
void instanced_group_init(InstancedGroup* group, const SphereInstance* data,
                          int count);

/**
 * @brief Binds a procedural mesh (e.g., icosphere) to the instanced group.
 * @param group Pointer to the group.
 * @param vbo Mesh vertex VBO.
 * @param nbo Mesh normal VBO.
 * @param ebo Mesh index EBO.
 */
void instanced_group_bind_mesh(InstancedGroup* group, GLuint vbo, GLuint nbo,
                               GLuint ebo);

/**
 * @brief Updates the instance VBO with new data (e.g., N-body positions).
 * @param group Pointer to the group.
 * @param data New instance data array.
 * @param count Number of instances (must not exceed original allocation).
 */
void instanced_group_update(InstancedGroup* group, const SphereInstance* data,
                            int count);

/**
 * @brief Executes an indexed instanced draw call.
 * @param group Pointer to the group.
 * @param index_count Number of indices in the base mesh.
 */
void instanced_group_draw(InstancedGroup* group, size_t index_count);

/**
 * @brief Releases all GPU resources for the instanced group.
 * @param group Pointer to the group.
 */
void instanced_group_cleanup(InstancedGroup* group);

#endif /* INSTANCED_RENDERING_H */
