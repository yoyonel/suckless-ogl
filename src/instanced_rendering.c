#include "instanced_rendering.h"

#include "gl_common.h"
#include "render_utils.h"
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

// Helper interne pour configurer les attributs d'instance
static void setup_instance_attributes(void)
{
	render_utils_setup_sphere_instance_attributes(
	    (GLsizei)sizeof(SphereInstance), offsetof(SphereInstance, albedo),
	    offsetof(SphereInstance, metallic));
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
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribDivisor(0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, nbo);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribDivisor(1, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

	/* -- INSTANCES (VBO Interne) -- */
	glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
	setup_instance_attributes();

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
