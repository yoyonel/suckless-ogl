#include "effects/fx_lut_viz.h"

#include "log.h"
#include "postprocess.h"
#include "shader.h"
#include <stdbool.h>
#include <stdlib.h>

enum LUTVizConstants { DEFAULT_LUT_VIZ_GRID_SIZE = 32 };

int fx_lut_viz_init(PostProcess* post_processing)
{
	LUTVizFX* viz = &post_processing->lut_viz_fx;
	viz->grid_size = DEFAULT_LUT_VIZ_GRID_SIZE;
	viz->is_enabled = false;

	/* Create point cloud grid */
	int num_points = viz->grid_size * viz->grid_size * viz->grid_size;
	float* points = (float*)malloc((size_t)num_points * 3 * sizeof(float));
	if (!points) {
		return -1;
	}

	int idx = 0;
	for (int iz = 0; iz < viz->grid_size; iz++) {
		for (int iy = 0; iy < viz->grid_size; iy++) {
			for (int ix = 0; ix < viz->grid_size; ix++) {
				points[idx++] =
				    (float)ix / (float)(viz->grid_size - 1);
				points[idx++] =
				    (float)iy / (float)(viz->grid_size - 1);
				points[idx++] =
				    (float)iz / (float)(viz->grid_size - 1);
			}
		}
	}

	glGenVertexArrays(1, &viz->vao);
	glBindVertexArray(viz->vao);

	glGenBuffers(1, &viz->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, viz->vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)((size_t)num_points * 3 * sizeof(float)),
	             points, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
	                      (void*)0);

	glBindVertexArray(0);
	free(points);

	/* Compile debug shaders */
	viz->shader = shader_load("shaders/debug/lut_viz.vert",
	                          "shaders/debug/lut_viz.frag");
	if (!viz->shader) {
		LOG_ERROR("suckless-ogl.postprocess.lut_viz",
		          "Failed to load LUT visualization shaders");
		return -1;
	}

	return 0;
}

void fx_lut_viz_cleanup(PostProcess* post_processing)
{
	LUTVizFX* viz = &post_processing->lut_viz_fx;
	if (viz->vao) {
		glDeleteVertexArrays(1, &viz->vao);
	}
	if (viz->vbo) {
		glDeleteBuffers(1, &viz->vbo);
	}
	if (viz->shader) {
		shader_destroy(viz->shader);
	}
}

void fx_lut_viz_render(PostProcess* post_processing)
{
	LUTVizFX* viz = &post_processing->lut_viz_fx;
	if (!viz->is_enabled || !post_processing->lut3d.texture) {
		return;
	}

	shader_use(viz->shader);

	/* Pass 3D LUT texture */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, post_processing->lut3d.texture);
	shader_set_int(viz->shader, "u_lut3d", 0);

	/* Set MVP with auto-rotation for visualization */
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 mvp;
	float time = (float)glfwGetTime();

	const float rot_y = 0.5F;
	const float rot_x = 0.3F;
	const float cam_z = 3.0F;
	const float fov = 45.0F;
	const float znear = 0.1F;
	const float zfar = 10.0F;

	glm_mat4_identity(model);
	glm_rotate(model, time * rot_y, (vec3){0.0F, 1.0F, 0.0F});
	glm_rotate(model, time * rot_x, (vec3){1.0F, 0.0F, 0.0F});

	glm_lookat((vec3){0.0F, 0.0F, cam_z}, (vec3){0.0F, 0.0F, 0.0F},
	           (vec3){0.0F, 1.0F, 0.0F}, view);
	glm_perspective(
	    glm_rad(fov),
	    (float)post_processing->width / (float)post_processing->height,
	    znear, zfar, proj);

	glm_mat4_mul(proj, view, mvp);
	glm_mat4_mul(mvp, model, mvp);

	shader_set_mat4(viz->shader, "u_mvp", (float*)mvp);

	glBindVertexArray(viz->vao);
	glDrawArrays(GL_POINTS, 0,
	             viz->grid_size * viz->grid_size * viz->grid_size);
	glBindVertexArray(0);

	glUseProgram(0);
}
