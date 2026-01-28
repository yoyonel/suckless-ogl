#include "instanced_rendering.h"

#include "gl_common.h"
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
	             GL_STATIC_DRAW);
}

// Helper interne pour configurer les attributs d'instance
static void setup_instance_attributes()
{
	GLsizei size_instance = (GLsizei)sizeof(SphereInstance);
	GLuint index_vattrib = 2;  // Start at 2 (0=Pos, 1=Norm usually)

	// mat4 model (Locations 2, 3, 4, 5)
	for (int i = 0; i < 4; i++) {
		glEnableVertexAttribArray(index_vattrib);
		glVertexAttribPointer(index_vattrib, 4, GL_FLOAT, GL_FALSE,
		                      size_instance,
		                      // NOLINTNEXTLINE(misc-include-cleaner)
		                      BUFFER_OFFSET(i * sizeof(vec4)));
		glVertexAttribDivisor(index_vattrib, 1);
		index_vattrib++;
	}
	// Albedo (6) + PBR (7)
	glEnableVertexAttribArray(index_vattrib);
	glVertexAttribPointer(index_vattrib, 3, GL_FLOAT, GL_FALSE,
	                      size_instance,
	                      BUFFER_OFFSET(offsetof(SphereInstance, albedo)));
	glVertexAttribDivisor(index_vattrib, 1);
	index_vattrib++;

	glEnableVertexAttribArray(index_vattrib);
	glVertexAttribPointer(
	    index_vattrib, 3, GL_FLOAT, GL_FALSE, size_instance,
	    BUFFER_OFFSET(offsetof(SphereInstance, metallic)));
	glVertexAttribDivisor(index_vattrib, 1);
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

void instanced_group_bind_billboard(InstancedGroup* group, GLuint vbo)
{
	if (group->vao != 0) {
		glDeleteVertexArrays(1, &group->vao);
		group->vao = 0;
	}

	glGenVertexArrays(1, &group->vao);
	glBindVertexArray(group->vao);

	// -- GÉOMÉTRIE (Quad) --
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribDivisor(0, 0);

	/* Ensure unused attribute 1 has divisor 0 */
	glVertexAttribDivisor(1, 0);

	// Pas de normales (index 1) pour les billboards

	// -- INSTANCES --
	glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
	setup_instance_attributes();

	glBindVertexArray(0);
}

void instanced_group_draw_arrays(InstancedGroup* group, GLenum mode, int first,
                                 int count)
{
	glBindVertexArray(group->vao);
	glDrawArraysInstanced(mode, first, count, group->instance_count);
	glBindVertexArray(0);
}

void instanced_group_draw(InstancedGroup* group, size_t index_count)
{
	glBindVertexArray(group->vao);
	glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)index_count,
	                        GL_UNSIGNED_INT, 0, group->instance_count);
	glBindVertexArray(0);
}

void instanced_group_cleanup(InstancedGroup* group)
{
	glDeleteBuffers(1, &group->instance_vbo);
	if (group->vao) {
		glDeleteVertexArrays(1, &group->vao);
	}
}
