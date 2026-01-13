#include "skybox.h"

/* Fullscreen quad vertices (NDC space) */
static const float quad_vertices[] = {
    -1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f,
    -1.0f, 1.0f, 0.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 1.0f,  0.0f,
};

void skybox_init(Skybox* skybox, GLuint shader_program)
{
	glGenVertexArrays(1, &skybox->vao);
	glGenBuffers(1, &skybox->vbo);

	/* Cache uniform locations */
	skybox->u_inv_view_proj = glGetUniformLocation(shader_program, "m_inv_view_proj");
	skybox->u_blur_lod = glGetUniformLocation(shader_program, "blur_lod");
	skybox->u_env_map = glGetUniformLocation(shader_program, "environmentMap");

	glBindVertexArray(skybox->vao);
	glBindBuffer(GL_ARRAY_BUFFER, skybox->vbo);

	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices,
	             GL_STATIC_DRAW);

	/* Position attribute */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
	                      (void*)0);

	glBindVertexArray(0);
}

void skybox_render(Skybox* skybox, GLuint shader_program, GLuint env_map,
                   const mat4 inv_view_proj, float blur_lod)
{
	/* Render behind everything else */
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_DEPTH_TEST);

	glUseProgram(shader_program);
    
	/* Set inverse view-projection matrix */
	glUniformMatrix4fv(skybox->u_inv_view_proj, 1, GL_FALSE, (const float*)inv_view_proj);

	/* Set blur LOD */
	glUniform1f(skybox->u_blur_lod, blur_lod);

	/* Bind environment map (equirectangular) */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, env_map);
	glUniform1i(skybox->u_env_map, 0);

	/* Draw fullscreen quad */
	glBindVertexArray(skybox->vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	/* Restore depth testing */
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
}

void skybox_cleanup(Skybox* skybox)
{
	glDeleteVertexArrays(1, &skybox->vao);
	glDeleteBuffers(1, &skybox->vbo);
}
