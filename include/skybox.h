#ifndef SKYBOX_H
#define SKYBOX_H

#include <cglm/cglm.h>

#include "gl_common.h"

typedef struct {
	GLuint vao;
	GLuint vbo;
} Skybox;

/* Initialize skybox geometry (fullscreen quad) */
void skybox_init(Skybox* skybox);

/* Render the skybox */
void skybox_render(Skybox* skybox, GLuint shader_program, GLuint env_map,
                   const mat4 inv_view_proj, float blur_lod);

/* Cleanup skybox resources */
void skybox_cleanup(Skybox* skybox);

#endif /* SKYBOX_H */
