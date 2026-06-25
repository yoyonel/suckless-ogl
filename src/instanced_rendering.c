#include "instanced_rendering.h"

#include "gl_common.h"
#include "render_utils.h"
#include "sphere_types.h"
#include <stddef.h>

void instanced_group_init(InstancedGroup* group, const SphereInstance* data,
                          int count)
{
	group->instance_count = count;
	group->vao = 0;  // Sera créé dans bind_mesh

	glGenBuffers(1, &group->instance_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(count * sizeof(SphereInstance)), data,
	             GL_DYNAMIC_DRAW);
}

void instanced_group_bind_mesh(InstancedGroup* group, GLuint vbo, GLuint nbo,
                               GLuint ebo)
{
	// Si on régénère l'icosphère, l'ancien VAO n'est plus valide
	if (group->vao != 0) {
		glDeleteVertexArrays(1, &group->vao);
		group->vao = 0;
	}

	glGenVertexArrays(1, &group->vao);
	glBindVertexArray(group->vao);

	// -- GÉOMÉTRIE (Empruntée à l'App) --
	glEnableVertexAttribArray(0);
	glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexAttribBinding(0, 0);

	glEnableVertexAttribArray(1);
	glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexAttribBinding(1, 1);

	glBindVertexBuffer(0, vbo, 0, 3 * sizeof(float));
	glBindVertexBuffer(1, nbo, 0, 3 * sizeof(float));

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

	/* -- INSTANCES (VBO Interne) -- */
	render_utils_setup_sphere_instance_attributes(
	    2, (GLsizei)sizeof(SphereInstance),
	    offsetof(SphereInstance, albedo),
	    offsetof(SphereInstance, metallic),
	    offsetof(SphereInstance, prev_center));
	glBindVertexBuffer(2, group->instance_vbo, 0,
	                   (GLsizei)sizeof(SphereInstance));

	/* CRITICAL: Explicitly disable and reset all higher slots (8-15)
	 * to ensure a stable global attribute signature on NVIDIA. */
	for (GLuint i = SYNC_ATTR_START; i < MAX_VERTEX_ATTRIBS_BASELINE; i++) {
		glDisableVertexAttribArray(i);
		glVertexAttribDivisor(i, 0);
	}

	glBindVertexArray(0);
}

void instanced_group_update(InstancedGroup* group, const SphereInstance* data,
                            int count)
{
	group->instance_count = count;
	glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0,
	                (GLsizeiptr)(count * sizeof(SphereInstance)), data);
}

void instanced_group_draw(InstancedGroup* group, size_t index_count)
{
	glBindVertexArray(group->vao);
	glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)index_count,
	                        GL_UNSIGNED_INT, 0, group->instance_count);
}

void instanced_group_cleanup(InstancedGroup* group)
{
	GL_SAFE_DELETE_BUFFER(group->instance_vbo);
	GL_SAFE_DELETE_VAO(group->vao);
}
