/**
 * @file billboard_rendering.h
 * @brief View-aligned quad rendering (billboarding) for optimized sphere
 * drawing.
 *
 * This module implements a billboarding system where spheres are rendered as
 * screen-aligned quads. This is significantly faster than rendering high-poly
 * meshes while maintaining high visual quality via fragment-shader raycasting.
 */

#ifndef BILLBOARD_RENDERING_H
#define BILLBOARD_RENDERING_H

#include "gl_common.h"
#include "instanced_rendering.h" /* For SphereInstance */
#include "shader.h"

/**
 * @struct BillboardGroup
 * @brief Manages a set of billboarded instances on the GPU.
 */
typedef struct {
	GLuint
	    vao; /**< Dedicated VAO linking quad geometry and instance data. */
	GLuint instance_vbo;  /**< GPU buffer storing per-instance attributes
	                         (position, color). */
	int instance_count;   /**< Number of billboards in this group. */
	GLuint vao_wire_quad; /**< VAO for debug wireframe quad. */
	GLuint vao_wire_box;  /**< VAO for debug wireframe box. */
} BillboardGroup;

/**
 * @brief Initializes the billboard group and allocates GPU memory.
 * @param group Pointer to the group.
 * @param data Initial instance data (can be NULL if only allocating).
 * @param count Number of instances to allocate for.
 */
void billboard_group_init(BillboardGroup* group, const SphereInstance* data,
                          int count);

/**
 * @brief Prepares the VAO by linking a shared quad VBO with internal instance
 * data.
 * @param group Pointer to the group.
 * @param quad_vbo VBO containing basic quad geometry (usually from
 * render_utils).
 */
void billboard_group_prepare(BillboardGroup* group, GLuint quad_vbo,
                             GLuint wire_quad_vbo, GLuint wire_cube_vbo);

/**
 * @brief Executes the instanced draw call for all billboards in the group.
 * @param group Pointer to the group.
 */
void billboard_group_draw(BillboardGroup* group);

/**
 * @brief Updates instance data on the GPU.
 *
 * Typically used for per-frame updates like sorting or movement.
 * @param group Pointer to the group.
 * @param data New instance data.
 * @param count Number of instances to update.
 */
void billboard_group_update(BillboardGroup* group, const SphereInstance* data,
                            int count);

/**
 * @brief Releases all GPU resources allocated for the billboard group.
 * @param group Pointer to the group.
 */
void billboard_group_cleanup(BillboardGroup* group);

/**
 * @brief Draws filled quads for each instance (for debug transparency).
 *
 * Assumes the correct shader is already bound.
 * @param group Pointer to the group.
 */
void billboard_group_draw_debug_fill(BillboardGroup* group);

/**
 * @brief Draws wireframe quads for each instance.
 *
 * Assumes the correct shader is already bound.
 * @param group Pointer to the group.
 */
void billboard_group_draw_debug_quads(BillboardGroup* group);

/**
 * @brief Draws wireframe boxes for each instance.
 *
 * Assumes the correct shader is already bound.
 * @param group Pointer to the group.
 */
void billboard_group_draw_debug_boxes(BillboardGroup* group);

#endif /* BILLBOARD_RENDERING_H */
