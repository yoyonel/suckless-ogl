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
#include <cglm/cglm.h>

/**
 * @struct SphereInstance
 * @brief Per-instance data sent to the shader via an instanced VBO.
 *
 * This structure is 64-byte aligned to ensure optimal GPU throughput
 * and compatibility with SIMD-based sorting or physics.
 */
typedef struct {
	mat4 model;      /**< 4x4 Transformation matrix. */
	vec3 albedo;     /**< Base color (linear RGB). */
	float metallic;  /**< PBR metallic factor (0.0 - 1.0). */
	float roughness; /**< PBR roughness factor (0.0 - 1.0). */
	float ao;        /**< Ambient occlusion factor. */
	float padding;   /**< Alignment padding. */
} __attribute__((aligned(SIMD_ALIGNMENT))) SphereInstance;

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
 * @brief Binds a quad mesh to the instanced group (fallback or specialized
 * use).
 * @param group Pointer to the group.
 * @param vbo Quad VBO.
 */
void instanced_group_bind_billboard(InstancedGroup* group, GLuint vbo);

/**
 * @brief Executes an indexed instanced draw call.
 * @param group Pointer to the group.
 * @param index_count Number of indices in the base mesh.
 */
void instanced_group_draw(InstancedGroup* group, size_t index_count);

/**
 * @brief Executes a non-indexed instanced draw call.
 * @param group Pointer to the group.
 * @param mode GL primitive mode (e.g., GL_TRIANGLES).
 * @param first Starting vertex index.
 * @param count Number of vertices per instance.
 */
void instanced_group_draw_arrays(InstancedGroup* group, GLenum mode, int first,
                                 int count);

/**
 * @brief Releases all GPU resources for the instanced group.
 * @param group Pointer to the group.
 */
void instanced_group_cleanup(InstancedGroup* group);

#endif /* INSTANCED_RENDERING_H */
