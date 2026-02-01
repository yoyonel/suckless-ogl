#include "app_scene.h"

#include "app.h"
#include "app_settings.h"
#include "billboard_rendering.h"
#include "glad/glad.h"
#include "instanced_rendering.h"
#include "log.h"
#include "material.h"
#include "render_utils.h"
#include "shader.h"
#include "sphere_sorting.h"
#include "utils.h"
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/vec3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifdef USE_SSBO_RENDERING
void app_init_ssbo(App* app)
{
	const int total_count =
	    MIN(app->material_lib->count, DEFAULT_COLS * DEFAULT_COLS);
	const int cols = DEFAULT_COLS;
	const int rows = (total_count + cols - 1) / cols;
	const float spacing = DEFAULT_SPACING;

	const float grid_w = (float)(cols - 1) * spacing;
	const float grid_h = (float)(rows - 1) * spacing;

	SphereInstanceSSBO* data =
	    malloc(sizeof(SphereInstanceSSBO) * (size_t)total_count);
	if (!data) {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to allocate memory for SSBO");
		return;
	}

	for (int i = 0; i < total_count; i++) {
		const int grid_x = i % cols;
		const int grid_y = i / cols;
		glm_mat4_identity(data[i].model);
		const float pos_x = ((float)grid_x * spacing) -
		                    (grid_w * HALF_OFFSET_MULTIPLIER);
		const float pos_y = -(((float)grid_y * spacing) -
		                      (grid_h * HALF_OFFSET_MULTIPLIER));
		vec3 position = {pos_x, pos_y, 0.0F};
		glm_translate(data[i].model, position);
		PBRMaterial* mat = &app->material_lib->materials[i];
		glm_vec3_copy(mat->albedo, data[i].albedo);
		data[i].metallic = mat->metallic;
		data[i].roughness = mat->roughness;
		data[i].ao = 1.0F;
		data[i]._padding[0] = 0.0F;
		data[i]._padding[1] = 0.0F;
	}

	ssbo_group_init(&app->ssbo_group, data, total_count);
	ssbo_group_bind_mesh(&app->ssbo_group, app->sphere_vbo, app->sphere_nbo,
	                     app->sphere_ebo);
	free(data);
}
#endif

void app_init_instancing(App* app)
{
	const int total_count =
	    MIN(app->material_lib->count, DEFAULT_COLS * DEFAULT_COLS);
	const int cols = DEFAULT_COLS;
	const int rows = (total_count + cols - 1) / cols;
	const float spacing = DEFAULT_SPACING;

	const float grid_w = (float)(cols - 1) * spacing;
	const float grid_h = (float)(rows - 1) * spacing;

	SphereInstance* data = NULL;
	// NOLINTNEXTLINE(misc-include-cleaner)
	if (posix_memalign((void**)&data, SIMD_ALIGNMENT,
	                   sizeof(SphereInstance) * (size_t)total_count) != 0) {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to allocate aligned memory for instancing");
		return;
	}

	for (int i = 0; i < total_count; i++) {
		const int grid_x = i % cols;
		const int grid_y = i / cols;
		glm_mat4_identity(data[i].model);
		const float pos_x = ((float)grid_x * spacing) -
		                    (grid_w * HALF_OFFSET_MULTIPLIER);
		const float pos_y = -(((float)grid_y * spacing) -
		                      (grid_h * HALF_OFFSET_MULTIPLIER));
		vec3 position = {pos_x, pos_y, 0.0F};
		// NOLINTNEXTLINE(misc-include-cleaner)
		glm_translate(data[i].model, position);
		PBRMaterial* mat = &app->material_lib->materials[i];
		glm_vec3_copy(mat->albedo, data[i].albedo);
		data[i].metallic = mat->metallic;
		data[i].roughness = mat->roughness;
		data[i].ao = 1.0F;
	}

	instanced_group_init(&app->instanced_group, data, total_count);

#ifdef USE_TRANSPARENT_BILLBOARDS
	// Use posix_memalign for consistency and to avoid implicit declaration
	// issues
	void* raw_mem = NULL;
	if (posix_memalign(&raw_mem, SIMD_ALIGNMENT,
	                   sizeof(SphereInstance) * (size_t)total_count) == 0) {
		app->sphere_instances = (SphereInstance*)raw_mem;
		safe_memcpy(app->sphere_instances,
		            sizeof(SphereInstance) * (size_t)total_count, data,
		            sizeof(SphereInstance) * (size_t)total_count);
		app->sphere_instance_count = total_count;
		sphere_sorter_init(&app->sphere_sorter, total_count);
	}
#endif

	instanced_group_bind_mesh(&app->instanced_group, app->sphere_vbo,
	                          app->sphere_nbo, app->sphere_ebo);
	billboard_group_init(&app->billboard_group, data, total_count);
	billboard_group_prepare(&app->billboard_group, app->quad_vbo);
	free(data);
}

void app_update_instancing_mode(App* app)
{
	(void)app;
}

void app_render_billboards(App* app, mat4 view, mat4 proj, vec3 camera_pos)
{
	Shader* current_shader = app->pbr_billboard_shader;
	shader_use(current_shader);

	render_utils_bind_texture_safe(GL_TEXTURE0, app->irradiance_tex,
	                               app->dummy_black_tex);
	render_utils_bind_texture_safe(GL_TEXTURE1, app->spec_prefiltered_tex,
	                               app->dummy_black_tex);
	render_utils_bind_texture_safe(GL_TEXTURE2, app->brdf_lut_tex,
	                               app->dummy_black_tex);

	shader_set_int(current_shader, "irradianceMap", 0);
	shader_set_int(current_shader, "prefilterMap", 1);
	shader_set_int(current_shader, "brdfLUT", 2);
	shader_set_int(current_shader, "debugMode", app->pbr_debug_mode);
	shader_set_vec3(current_shader, "camPos", camera_pos);
	shader_set_mat4(current_shader, "projection", (float*)proj);
	shader_set_mat4(current_shader, "view", (float*)view);
	shader_set_mat4(
	    current_shader, "previousViewProj",
	    (float*)app->postprocess.motion_blur_fx.previous_view_proj);

	float screen_size[2] = {(float)app->width, (float)app->height};
	shader_set_vec2(current_shader, "u_screenSize", screen_size);

	billboard_group_draw(&app->billboard_group);
}

void app_render_instanced(App* app, mat4 view, mat4 proj, vec3 camera_pos)
{
	Shader* current_shader = NULL;
#ifdef USE_SSBO_RENDERING
	current_shader = app->pbr_ssbo_shader;
#else
	current_shader = app->pbr_instanced_shader;
#endif

	shader_use(current_shader);

	render_utils_bind_texture_safe(GL_TEXTURE0, app->irradiance_tex,
	                               app->dummy_black_tex);
	render_utils_bind_texture_safe(GL_TEXTURE1, app->spec_prefiltered_tex,
	                               app->dummy_black_tex);
	render_utils_bind_texture_safe(GL_TEXTURE2, app->brdf_lut_tex,
	                               app->dummy_black_tex);

	shader_set_int(current_shader, "irradianceMap", 0);
	shader_set_int(current_shader, "prefilterMap", 1);
	shader_set_int(current_shader, "brdfLUT", 2);
	shader_set_int(current_shader, "debugMode", app->pbr_debug_mode);
	shader_set_vec3(current_shader, "camPos", camera_pos);
	shader_set_mat4(current_shader, "projection", (float*)proj);
	shader_set_mat4(current_shader, "view", (float*)view);
	shader_set_mat4(
	    current_shader, "previousViewProj",
	    (float*)app->postprocess.motion_blur_fx.previous_view_proj);

#ifdef USE_SSBO_RENDERING
	ssbo_group_draw(&app->ssbo_group, app->geometry.indices.size);
#else
	instanced_group_draw(&app->instanced_group,
	                     (int)app->geometry.indices.size);
#endif
}

void app_update_gpu_buffers(App* app)
{
	glBindVertexArray(app->sphere_vao);
	glBindBuffer(GL_ARRAY_BUFFER, app->sphere_vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(app->geometry.vertices.size * sizeof(vec3)),
	             app->geometry.vertices.data, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, app->sphere_nbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(app->geometry.normals.size * sizeof(vec3)),
	             app->geometry.normals.data, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->sphere_ebo);
	glBufferData(
	    GL_ELEMENT_ARRAY_BUFFER,
	    (GLsizeiptr)(app->geometry.indices.size * sizeof(unsigned int)),
	    app->geometry.indices.data, GL_STATIC_DRAW);
	glBindVertexArray(0);
}
