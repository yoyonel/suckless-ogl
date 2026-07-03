#include "billboard_renderer.h"

#include "billboard_sorter.h"
#include "bool_utils.h"
#include "gl_common.h"
#include "render_utils.h"
#include "scene_uniforms.h"
#include "sphere_types.h"
#include "utils.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

static const int WIRE_CUBE_VERTEX_COUNT = 24;

static void create_billboard_vao(GLuint* vao, GLuint geometry_vbo,
                                 GLuint instance_vbo)
{
	if (*vao != 0) {
		glDeleteVertexArrays(1, vao);
		*vao = 0;
	}

	glGenVertexArrays(1, vao);
	glBindVertexArray(*vao);

	/* -- GEOMETRY -- */
	glEnableVertexAttribArray(0);
	glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexAttribBinding(0, 0);

	glEnableVertexAttribArray(1);
	glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexAttribBinding(1, 1);

	glBindVertexBuffer(0, geometry_vbo, 0, 3 * sizeof(float));
	glBindVertexBuffer(1, geometry_vbo, 0, 3 * sizeof(float));

	/* -- INSTANCES -- */
	render_utils_setup_sphere_instance_attributes(
	    2, (GLsizei)sizeof(SphereInstance),
	    offsetof(SphereInstance, albedo),
	    offsetof(SphereInstance, metallic),
	    offsetof(SphereInstance, prev_center));
	glBindVertexBuffer(2, instance_vbo, 0, (GLsizei)sizeof(SphereInstance));

	/* Explicitly disable higher slots */
	for (GLuint i = SYNC_ATTR_START; i < MAX_VERTEX_ATTRIBS_BASELINE; i++) {
		glDisableVertexAttribArray(i);
		glVertexAttribDivisor(i, 0);
	}

	glBindVertexArray(0);
}

void billboard_renderer_init(BillboardRenderer* renderer, int initial_capacity)
{
	assert(renderer->instance_vbo == 0 && "Double initialization detected");

	renderer->instance_count = 0;
	renderer->capacity = initial_capacity;
	renderer->vao = 0;
	renderer->vao_wire_quad = 0;
	renderer->vao_wire_box = 0;
	renderer->cached_debug_program = 0;

	renderer->loc_proj = -1;
	renderer->loc_view = -1;
	renderer->loc_stippled = -1;
	renderer->loc_billboard_mode = -1;
	renderer->loc_use_instance_col = -1;
	renderer->loc_color = -1;

	/* Initialize the nested sorter */
	billboard_sorter_init(&renderer->sorter, initial_capacity);

	/* Create and allocate GPU instance buffer */
	glGenBuffers(1, &renderer->instance_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->instance_vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(initial_capacity * sizeof(SphereInstance)),
	             NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glObjectLabel(GL_BUFFER, renderer->instance_vbo, -1,
	              "BillboardRenderer VBO");
}

void billboard_renderer_prepare(BillboardRenderer* renderer, GLuint quad_vbo,
                                GLuint wire_quad_vbo, GLuint wire_cube_vbo)
{
	create_billboard_vao(&renderer->vao, quad_vbo, renderer->instance_vbo);
	create_billboard_vao(&renderer->vao_wire_quad, wire_quad_vbo,
	                     renderer->instance_vbo);
	create_billboard_vao(&renderer->vao_wire_box, wire_cube_vbo,
	                     renderer->instance_vbo);
}

static void billboard_renderer_update_from_buffer(BillboardRenderer* renderer,
                                                  GLuint src_buffer, int count)
{
	if (renderer->instance_vbo == 0) {
		return;
	}

	renderer->instance_count = count;
	GLsizeiptr size = (GLsizeiptr)(count * sizeof(SphereInstance));

	glBindBuffer(GL_COPY_READ_BUFFER, src_buffer);
	glBindBuffer(GL_COPY_WRITE_BUFFER, renderer->instance_vbo);

	if (count > renderer->capacity) {
		/* Reallocate buffer */
		glBufferData(GL_COPY_WRITE_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
		renderer->capacity = count;
	}

	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0,
	                    size);
	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

static void update_billboard_ubo(const BillboardRenderParams* params,
                                 void* billboard_ubo_ptr)
{
	if (!billboard_ubo_ptr) {
		return;
	}

	BillboardUBO ubo = {0};
	(void)safe_memcpy(ubo.projection, sizeof(ubo.projection),
	                  params->projection, sizeof(mat4));
	(void)safe_memcpy(ubo.view, sizeof(ubo.view), params->view,
	                  sizeof(mat4));
	(void)safe_memcpy(ubo.previous_view_proj,
	                  sizeof(ubo.previous_view_proj),
	                  params->previous_view_proj, sizeof(mat4));
	glm_vec3_copy((float*)params->camera_pos, ubo.cam_pos);
	ubo.debug_mode = params->pbr_debug_mode;
	ubo.screen_size[0] = params->screen_size[0];
	ubo.screen_size[1] = params->screen_size[1];
	glm_vec3_copy((float*)params->probe_grid_min, ubo.probe_grid_min);
	ubo.gi_mode = params->gi_mode;
	glm_vec3_copy((float*)params->probe_grid_max, ubo.probe_grid_max);
	ubo.specular_aa_enabled = BOOL_TO_INT(params->specular_aa_enabled);
	ubo.probe_grid_dim[0] = params->probe_grid_dim[0];
	ubo.probe_grid_dim[1] = params->probe_grid_dim[1];
	ubo.probe_grid_dim[2] = params->probe_grid_dim[2];
	ubo.aa_mode = params->aa_mode;

	*(BillboardUBO*)billboard_ubo_ptr = ubo;
}

static void draw_primary_pass(BillboardRenderer* renderer,
                              GLuint pbr_billboard_shader, int instance_count)
{
	glUseProgram(pbr_billboard_shader);
	renderer->instance_count = instance_count;

	if (renderer->vao != 0) {
		glBindVertexArray(renderer->vao);

		GLboolean culling_was_enabled = glIsEnabled(GL_CULL_FACE);
		glDisable(GL_CULL_FACE);

		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
		                      renderer->instance_count);

		if (culling_was_enabled != 0U) {
			glEnable(GL_CULL_FACE);
		}
	}
}

static void update_debug_uniform_locations(BillboardRenderer* renderer,
                                           GLuint debug_line_shader)
{
	if (debug_line_shader != renderer->cached_debug_program) {
		renderer->cached_debug_program = debug_line_shader;
		renderer->loc_proj =
		    glGetUniformLocation(debug_line_shader, "projection");
		renderer->loc_view =
		    glGetUniformLocation(debug_line_shader, "view");
		renderer->loc_stippled =
		    glGetUniformLocation(debug_line_shader, "u_stippled");
		renderer->loc_billboard_mode =
		    glGetUniformLocation(debug_line_shader, "u_billboardMode");
		renderer->loc_use_instance_col = glGetUniformLocation(
		    debug_line_shader, "u_useInstanceColor");
		renderer->loc_color =
		    glGetUniformLocation(debug_line_shader, "u_color");
	}
}

static void draw_debug_overlays(BillboardRenderer* renderer,
                                const BillboardRenderParams* params,
                                GLuint debug_line_shader, GLuint sorted_ssbo)
{
	if (params->pbr_debug_mode != 0 || !params->wireframe) {
		return;
	}

	/* Legacy VBO copy only for debug wireframe overlay */
	billboard_renderer_update_from_buffer(renderer, sorted_ssbo,
	                                      params->instance_count);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glUseProgram(debug_line_shader);

	update_debug_uniform_locations(renderer, debug_line_shader);

	if (renderer->loc_proj >= 0) {
		glUniformMatrix4fv(renderer->loc_proj, 1, GL_FALSE,
		                   (const float*)params->projection);
	}
	if (renderer->loc_view >= 0) {
		glUniformMatrix4fv(renderer->loc_view, 1, GL_FALSE,
		                   (const float*)params->view);
	}

	const float debug_fill_alpha = 0.10F;
	const float debug_box_alpha = 0.5F;
	const float debug_offset_fill = 1.0F;
	const float debug_offset_line = -2.0F;

	/* 4a. Transparent Fill */
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(debug_offset_fill, debug_offset_fill);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (renderer->loc_stippled >= 0) {
		glUniform1i(renderer->loc_stippled, 0);
	}
	if (renderer->loc_billboard_mode >= 0) {
		glUniform1i(renderer->loc_billboard_mode, 1);
	}
	if (renderer->loc_use_instance_col >= 0) {
		glUniform1i(renderer->loc_use_instance_col, 1);
	}

	float color_fill[4] = {1.0F, 1.0F, 1.0F, debug_fill_alpha};
	if (renderer->loc_color >= 0) {
		glUniform4fv(renderer->loc_color, 1, color_fill);
	}

	if (renderer->vao != 0) {
		glBindVertexArray(renderer->vao);
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
		                      renderer->instance_count);
	}

	glDisable(GL_BLEND);
	glDisable(GL_POLYGON_OFFSET_FILL);

	/* 4b. Quad Outline (Solid Green/White) */
	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(debug_offset_line, debug_offset_line);

	if (renderer->loc_stippled >= 0) {
		glUniform1i(renderer->loc_stippled, 0);
	}
	if (renderer->loc_billboard_mode >= 0) {
		glUniform1i(renderer->loc_billboard_mode, 1);
	}
	if (renderer->loc_use_instance_col >= 0) {
		glUniform1i(renderer->loc_use_instance_col, 0);
	}

	float color_quad[4] = {0.0F, 1.0F, 0.0F, 1.0F};
	if (renderer->loc_color >= 0) {
		glUniform4fv(renderer->loc_color, 1, color_quad);
	}

	if (renderer->vao_wire_quad != 0) {
		glBindVertexArray(renderer->vao_wire_quad);
		glDrawArraysInstanced(GL_LINE_LOOP, 0, 4,
		                      renderer->instance_count);
	}

	/* 4c. Bounding Box (Dotted/Stippled Red/Yellow) */
	if (renderer->loc_stippled >= 0) {
		glUniform1i(renderer->loc_stippled, 1);
	}
	if (renderer->loc_billboard_mode >= 0) {
		glUniform1i(renderer->loc_billboard_mode, 0);
	}

	float color_box[4] = {1.0F, 1.0F, 0.0F, debug_box_alpha};
	if (renderer->loc_color >= 0) {
		glUniform4fv(renderer->loc_color, 1, color_box);
	}

	if (renderer->vao_wire_box != 0) {
		glBindVertexArray(renderer->vao_wire_box);
		glDrawArraysInstanced(GL_LINES, 0, WIRE_CUBE_VERTEX_COUNT,
		                      renderer->instance_count);
	}

	glDisable(GL_POLYGON_OFFSET_LINE);
	glDepthMask(GL_TRUE);
}

void billboard_renderer_draw(BillboardRenderer* renderer,
                             const BillboardRenderParams* params,
                             GLuint pbr_billboard_shader,
                             GLuint debug_line_shader, void* billboard_ubo_ptr)
{
	if (params->instance_count <= 0) {
		return;
	}

	/* 1. Sort the billboards and bind the sorted SSBO */
	GLuint sorted_ssbo = billboard_sorter_sort(
	    &renderer->sorter, params->instances, params->instance_count,
	    params->camera_pos, params->sorting_mode);

	/* 2. Populate and update BillboardUBO */
	update_billboard_ubo(params, billboard_ubo_ptr);

	/* 3. Execute primary PBR billboard drawing */
	draw_primary_pass(renderer, pbr_billboard_shader,
	                  params->instance_count);

	/* 4. Draw debug overlays if requested */
	draw_debug_overlays(renderer, params, debug_line_shader, sorted_ssbo);
}

void billboard_renderer_cleanup(BillboardRenderer* renderer)
{
	billboard_sorter_cleanup(&renderer->sorter);

	GL_SAFE_DELETE_VAO(renderer->vao);
	GL_SAFE_DELETE_VAO(renderer->vao_wire_quad);
	GL_SAFE_DELETE_VAO(renderer->vao_wire_box);
	GL_SAFE_DELETE_BUFFER(renderer->instance_vbo);

	renderer->instance_count = 0;
	renderer->capacity = 0;
}
