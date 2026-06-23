#include "skybox.h"

#include "gl_common.h"
#include "glad/glad.h"
#include "render_utils.h"
#include "shader.h"
#include <cglm/types.h>

void skybox_init(Skybox* skybox, Shader* shader)
{
	/* Cache uniform locations using robust shader API */
	skybox->u_inv_view_proj =
	    shader_get_uniform_location(shader, "m_inv_view_proj");
	skybox->u_blur_lod = shader_get_uniform_location(shader, "blur_lod");
	skybox->u_env_map =
	    shader_get_uniform_location(shader, "environmentMap");

	/* Use shared fullscreen quad creation */
	/* Note: This creates a VBO with (Pos: vec2, Tex: vec2) */
	/* Shader expects (Pos: vec3), but only reads xy. Z defaults to 0. */
	render_utils_create_fullscreen_quad(&skybox->vao, &skybox->vbo);

	glBindVertexArray(skybox->vao);

	/* CRITICAL: Explicitly disable other attributes to avoid
	 * driver-specific recompilation heuristics.
	 * render_utils enables 0 (Pos) and 1 (Tex). We disable 1 and others. */
	static const GLuint MAX_ATTR_RECONCILE = 7;
	for (GLuint i = 1; i <= MAX_ATTR_RECONCILE; i++) {
		glDisableVertexAttribArray(i);
		glVertexAttribDivisor(i, 0);
	}

	glBindVertexArray(0);
}

void skybox_render(Skybox* skybox, Shader* shader, GLuint env_map,
                   GLuint fallback_tex, const mat4 inv_view_proj,
                   float blur_lod)
{
	/* Render at the far plane (z=1.0) with LEQUAL depth test */
	glDepthFunc(GL_LEQUAL);

	shader_use(shader);

	/* Set inverse view-projection matrix */
	glUniformMatrix4fv(skybox->u_inv_view_proj, 1, GL_FALSE,
	                   (const float*)inv_view_proj);

	/* Set blur LOD */
	glUniform1f(skybox->u_blur_lod, blur_lod);

	/* Bind environment map (equirectangular) */
	glActiveTexture(GL_TEXTURE0);
	if (env_map) {
		glBindTexture(GL_TEXTURE_2D, env_map);
	} else {
		glBindTexture(GL_TEXTURE_2D, fallback_tex);
	}
	glUniform1i(skybox->u_env_map, 0);

	/* Draw fullscreen quad */
	glBindVertexArray(skybox->vao);
	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

	/* Restore default depth test */
	glDepthFunc(GL_LESS);
}

void skybox_cleanup(Skybox* skybox)
{
	GL_SAFE_DELETE_VAO(skybox->vao);
	GL_SAFE_DELETE_BUFFER(skybox->vbo);
}
