#include "billboard_rendering.h"

#include "gl_common.h"
#include "instanced_rendering.h"
#include <cglm/types.h>
#include <stddef.h>

void billboard_group_init(BillboardGroup* group, const SphereInstance* data,
                          int count)
{
	group->instance_count = count;
	group->vao = 0;

	/* Create and upload instance buffer */
	glGenBuffers(1, &group->instance_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(count * sizeof(SphereInstance)), data,
	             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void setup_billboard_instance_attributes()
{
	GLsizei size_instance = (GLsizei)sizeof(SphereInstance);
	GLuint index_vattrib = 2; /* Start at 2 (0=Pos, 1=unused/Normal) */

	/* mat4 model (Locations 2, 3, 4, 5) */
	for (int i = 0; i < 4; i++) {
		glEnableVertexAttribArray(index_vattrib);
		glVertexAttribPointer(index_vattrib, 4, GL_FLOAT, GL_FALSE,
		                      size_instance,
		                      BUFFER_OFFSET(i * sizeof(vec4)));
		glVertexAttribDivisor(index_vattrib, 1);
		index_vattrib++;
	}

	/* Albedo (6) */
	glEnableVertexAttribArray(index_vattrib);
	glVertexAttribPointer(index_vattrib, 3, GL_FLOAT, GL_FALSE,
	                      size_instance,
	                      BUFFER_OFFSET(offsetof(SphereInstance, albedo)));
	glVertexAttribDivisor(index_vattrib, 1);
	index_vattrib++;

	/* PBR (7) */
	glEnableVertexAttribArray(index_vattrib);
	glVertexAttribPointer(
	    index_vattrib, 3, GL_FLOAT, GL_FALSE, size_instance,
	    BUFFER_OFFSET(offsetof(SphereInstance, metallic)));
	glVertexAttribDivisor(index_vattrib, 1);
}

void billboard_group_prepare(BillboardGroup* group, GLuint quad_vbo)
{
	if (group->vao != 0) {
		glDeleteVertexArrays(1, &group->vao);
		group->vao = 0;
	}

	glGenVertexArrays(1, &group->vao);
	glBindVertexArray(group->vao);

	/* -- GEOMETRY (Quad) -- */
	glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);

	/* Layout 0: Position (vec3) */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	/* CRITICAL: Explicitly set divisor to 0 for geometry attributes */
	glVertexAttribDivisor(0, 0);

	/* -- INSTANCES -- */
	glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
	setup_billboard_instance_attributes();

	glBindVertexArray(0);
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
}
