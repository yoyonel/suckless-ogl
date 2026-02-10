#include "billboard_rendering.h"

#include "gl_common.h"
#include "instanced_rendering.h"
#include "render_utils.h"
#include "shader.h"
#include <cglm/types.h>
#include <stddef.h>

static const int WIRE_CUBE_VERTEX_COUNT = 24;

void billboard_group_init(BillboardGroup* group, const SphereInstance* data,
                          int count)
{
	group->instance_count = count;
	group->vao = 0;
	group->vao_wire_quad = 0;
	group->vao_wire_box = 0;

	/* Create and upload instance buffer */
	glGenBuffers(1, &group->instance_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(count * sizeof(SphereInstance)), data,
	             GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void billboard_group_update(BillboardGroup* group, const SphereInstance* data,
                            int count)
{
	if (group->instance_vbo == 0) {
		return;
	}

	/* Update GPU buffer with new sorted data */
	glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
	/* Using glBufferSubData is fine for small updates (10-100 instances) */
	glBufferSubData(GL_ARRAY_BUFFER, 0,
	                (GLsizeiptr)(count * sizeof(SphereInstance)), data);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void setup_billboard_instance_attributes(void)
{
	render_utils_setup_sphere_instance_attributes(
	    (GLsizei)sizeof(SphereInstance), offsetof(SphereInstance, albedo),
	    offsetof(SphereInstance, metallic));
}

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
	glBindBuffer(GL_ARRAY_BUFFER, geometry_vbo);

	/* Layout 0: Position (vec3) */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(0, 0);

	/* Layout 1: Normals (unused but consistently defined) */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(1, 0);

	/* -- INSTANCES -- */
	glBindBuffer(GL_ARRAY_BUFFER, instance_vbo);
	setup_billboard_instance_attributes();

	/* Explicitly disable higher slots */
	for (GLuint i = SYNC_ATTR_START; i < MAX_VERTEX_ATTRIBS_BASELINE; i++) {
		glDisableVertexAttribArray(i);
		glVertexAttribDivisor(i, 0);
	}

	glBindVertexArray(0);
}

void billboard_group_prepare(BillboardGroup* group, GLuint quad_vbo,
                             GLuint wire_quad_vbo, GLuint wire_cube_vbo)
{
	create_billboard_vao(&group->vao, quad_vbo, group->instance_vbo);
	create_billboard_vao(&group->vao_wire_quad, wire_quad_vbo,
	                     group->instance_vbo);
	create_billboard_vao(&group->vao_wire_box, wire_cube_vbo,
	                     group->instance_vbo);
}

void billboard_group_draw(BillboardGroup* group)
{
	if (group->vao == 0) {
		return;
	}

	glBindVertexArray(group->vao);

	/* Save previous Cull Face state */
	GLboolean culling_was_enabled = glIsEnabled(GL_CULL_FACE);

	/* Disable Face Culling for billboards to ensure visibility */
	glDisable(GL_CULL_FACE);

	/* Draw 4 vertices (Triangle Strip) -> 2 triangles (Quad) */
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, group->instance_count);

	/* Restore Face Culling only if it was enabled */
	if (culling_was_enabled) {
		glEnable(GL_CULL_FACE);
	}

	glBindVertexArray(0);
}

void billboard_group_cleanup(BillboardGroup* group)
{
	if (group->instance_vbo) {
		glDeleteBuffers(1, &group->instance_vbo);
		group->instance_vbo = 0;
	}
	if (group->vao) {
		glDeleteVertexArrays(1, &group->vao);
		group->vao = 0;
	}
	if (group->vao_wire_quad) {
		glDeleteVertexArrays(1, &group->vao_wire_quad);
		group->vao_wire_quad = 0;
	}
	if (group->vao_wire_box) {
		glDeleteVertexArrays(1, &group->vao_wire_box);
		group->vao_wire_box = 0;
	}
}

void billboard_group_draw_debug_fill(BillboardGroup* group)
{
	if (group->vao == 0) {
		return;
	}

	glBindVertexArray(group->vao);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, group->instance_count);
	glBindVertexArray(0);
}

void billboard_group_draw_debug_quads(BillboardGroup* group)
{
	if (group->vao_wire_quad == 0) {
		return;
	}

	glBindVertexArray(group->vao_wire_quad);
	// 4 vertices for a quad (GL_LINE_LOOP or GL_LINES depending on
	// VBO) render_utils uses GL_LINE_LOOP implicitly by order? No,
	// it's 4 verts. If GL_LINE_LOOP, we need
	// glDrawArraysInstanced(GL_LINE_LOOP, ...) But
	// render_utils_create_wire_quad_vbo puts 4 vertices. We'll use
	// GL_LINE_LOOP to close it.
	glDrawArraysInstanced(GL_LINE_LOOP, 0, 4, group->instance_count);
	glBindVertexArray(0);
}

void billboard_group_draw_debug_boxes(BillboardGroup* group)
{
	if (group->vao_wire_box == 0) {
		return;
	}

	glBindVertexArray(group->vao_wire_box);
	// Cube VBO has 24 vertices (GL_LINES)
	glDrawArraysInstanced(GL_LINES, 0, WIRE_CUBE_VERTEX_COUNT,
	                      group->instance_count);
	glBindVertexArray(0);
}
