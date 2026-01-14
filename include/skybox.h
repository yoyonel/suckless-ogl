#ifndef SKYBOX_H
#define SKYBOX_H

#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/types.h>

#include "gl_common.h"

typedef struct {
	GLuint vao;
	GLuint vbo;

	/* Cached uniform locations */
	GLint u_inv_view_proj;
	GLint u_blur_lod;
	GLint u_env_map;
} Skybox;

/* Initialize skybox geometry (fullscreen quad) */
void skybox_init(Skybox* skybox, GLuint shader_program);

/* Render the skybox */
void skybox_render(Skybox* skybox, GLuint shader_program, GLuint env_map,
                   const mat4 inv_view_proj, float blur_lod);

/* Cleanup skybox resources */
void skybox_cleanup(Skybox* skybox);

#endif /* SKYBOX_H */
