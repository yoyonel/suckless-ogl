/**
 * @file billboard_renderer.h
 * @brief Unified billboard sorting and rendering system.
 */

#ifndef BILLBOARD_RENDERER_H
#define BILLBOARD_RENDERER_H

#include "billboard_sorter.h"
#include "gl_common.h"
#include "scene_config.h"
#include "sphere_types.h"
#include <cglm/cglm.h>

/**
 * @struct BillboardRenderParams
 * @brief Parameters required to execute a billboard rendering pass, decoupled
 * from Scene.
 */
typedef struct {
	mat4 projection;
	mat4 view;
	mat4 previous_view_proj;
	vec3 camera_pos;
	int32_t pbr_debug_mode;
	float screen_size[2];
	vec3 probe_grid_min;
	vec3 probe_grid_max;
	int32_t probe_grid_dim[3];
	int32_t gi_mode;
	bool specular_aa_enabled;
	int32_t aa_mode;

	/* Configuration flags for drawing */
	bool wireframe;
	SortingMode sorting_mode;

	/* Active instances to sort and render */
	const SphereInstance* instances;
	int instance_count;
} BillboardRenderParams;

/**
 * @struct BillboardRenderer
 * @brief Manages billboard rendering buffers, VAO state, and the internal
 * sorter.
 */
typedef struct {
	GLuint
	    vao; /**< Dedicated VAO linking quad geometry and instance data. */
	GLuint instance_vbo; /**< GPU buffer storing per-instance attributes. */
	int instance_count; /**< Current number of billboard instances drawn. */
	int capacity;       /**< Current VBO capacity. */
	GLuint vao_wire_quad; /**< VAO for debug wireframe quad. */
	GLuint vao_wire_box;  /**< VAO for debug wireframe box. */

	/* Nested BillboardSorter */
	BillboardSorter sorter;

	/* Cached uniform locations for debug_line shader to prevent per-frame
	 * lookups */
	GLuint cached_debug_program;
	GLint loc_proj;
	GLint loc_view;
	GLint loc_stippled;
	GLint loc_billboard_mode;
	GLint loc_use_instance_col;
	GLint loc_color;
} BillboardRenderer;

/**
 * @brief Initializes the billboard renderer and allocates internal sorter
 * buffers.
 * @param renderer Pointer to the renderer struct.
 * @param initial_capacity Expected number of instances.
 */
void billboard_renderer_init(BillboardRenderer* renderer, int initial_capacity);

/**
 * @brief Prepares the VAO by linking the shared quad, wire quad, and wire cube
 * VBOs.
 * @param renderer Pointer to the renderer struct.
 * @param quad_vbo VBO containing basic quad geometry.
 * @param wire_quad_vbo VBO for debug wireframe quad.
 * @param wire_cube_vbo VBO for debug wireframe unit box.
 */
void billboard_renderer_prepare(BillboardRenderer* renderer, GLuint quad_vbo,
                                GLuint wire_quad_vbo, GLuint wire_cube_vbo);

/**
 * @brief Executes the sorting, UBO upload, and drawing commands for all
 * billboards.
 * @param renderer               Pointer to the renderer struct.
 * @param params                 Stateless parameter block.
 * @param pbr_billboard_shader   Shader used for the primary PBR pass.
 * @param debug_line_shader      Shader used for the wireframe debug overlays.
 * @param billboard_ubo_ptr      Mapped UBO pointer to copy the BillboardUBO
 * struct to.
 */
void billboard_renderer_draw(BillboardRenderer* renderer,
                             const BillboardRenderParams* params,
                             GLuint pbr_billboard_shader,
                             GLuint debug_line_shader, void* billboard_ubo_ptr);

/**
 * @brief Releases all GPU resources allocated for the billboard renderer.
 * @param renderer Pointer to the renderer struct.
 */
void billboard_renderer_cleanup(BillboardRenderer* renderer);

#endif /* BILLBOARD_RENDERER_H */
