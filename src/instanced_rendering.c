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

	glBindBuffer(GL_ARRAY_BUFFER, nbo);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

	// -- INSTANCES (VBO Interne) --
	glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
	GLsizei size_instance = (GLsizei)sizeof(SphereInstance);

	GLuint index_generic_vertex_attribute = 2;
	// mat4 model (Locations 2, 3, 4, 5)
	for (int i = 0; i < 4; i++) {
		glEnableVertexAttribArray(index_generic_vertex_attribute);
		glVertexAttribPointer(index_generic_vertex_attribute, 4,
		                      GL_FLOAT, GL_FALSE, size_instance,
		                      // NOLINTNEXTLINE(misc-include-cleaner)
		                      BUFFER_OFFSET(i * sizeof(vec4)));
		glVertexAttribDivisor(index_generic_vertex_attribute, 1);
		index_generic_vertex_attribute++;
	}
	// Albedo (6) + PBR (7)
	glEnableVertexAttribArray(index_generic_vertex_attribute);
	glVertexAttribPointer(index_generic_vertex_attribute, 3, GL_FLOAT,
	                      GL_FALSE, size_instance,
	                      BUFFER_OFFSET(offsetof(SphereInstance, albedo)));
	glVertexAttribDivisor(index_generic_vertex_attribute, 1);
	index_generic_vertex_attribute++;

	glEnableVertexAttribArray(index_generic_vertex_attribute);
	glVertexAttribPointer(
	    index_generic_vertex_attribute, 3, GL_FLOAT, GL_FALSE,
	    size_instance, BUFFER_OFFSET(offsetof(SphereInstance, metallic)));
	glVertexAttribDivisor(index_generic_vertex_attribute, 1);
	// index_generic_vertex_attribute++;

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