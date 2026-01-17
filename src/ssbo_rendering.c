#include "ssbo_rendering.h"

#include "gl_common.h"
#include "log.h"
#include <stddef.h>

void ssbo_group_init(SSBOGroup* group, const SphereInstanceSSBO* data,
                     int count)
{
	group->instance_count = count;
	group->vao = 0;

	/* Création du SSBO */
	glGenBuffers(1, &group->ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, group->ssbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
	             (GLsizeiptr)(count * sizeof(SphereInstanceSSBO)), data,
	             GL_STATIC_DRAW);

	/* IMPORTANT : Binding au point 0 */
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, group->ssbo);

	/* Vérification OpenGL */
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		LOG_ERROR("suckless-ogl.ssbo",
		          "OpenGL error after SSBO init: 0x%x", error);
	}

	LOG_INFO("suckless-ogl.ssbo",
	         "SSBO initialized: %d instances (%zu bytes), buffer ID: %u",
	         count, count * sizeof(SphereInstanceSSBO), group->ssbo);
}

void ssbo_group_bind_mesh(SSBOGroup* group, GLuint vbo, GLuint nbo, GLuint ebo)
{
	/* Si on régénère l'icosphère, l'ancien VAO n'est plus valide */
	if (group->vao != 0) {
		glDeleteVertexArrays(1, &group->vao);
		group->vao = 0;
	}

	glGenVertexArrays(1, &group->vao);
	glBindVertexArray(group->vao);

	/* Géométrie de base (positions) */
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);

	/* Normales */
	glBindBuffer(GL_ARRAY_BUFFER, nbo);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(1);

	/* Indices */
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

	glBindVertexArray(0);
}

void ssbo_group_draw(SSBOGroup* group, size_t index_count)
{
	/* IMPORTANT : Re-bind le SSBO avant le draw (au cas où) */
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, group->ssbo);

	glBindVertexArray(group->vao);
	glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)index_count,
	                        GL_UNSIGNED_INT, 0, group->instance_count);
	glBindVertexArray(0);
}

void ssbo_group_cleanup(SSBOGroup* group)
{
	if (group->ssbo) {
		glDeleteBuffers(1, &group->ssbo);
		group->ssbo = 0;
	}
	if (group->vao) {
		glDeleteVertexArrays(1, &group->vao);
		group->vao = 0;
	}
}