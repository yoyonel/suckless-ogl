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
	billboard_group_prepare(&app->billboard_group, app->quad_vbo,
	                        app->wire_quad_vbo, app->wire_cube_vbo);
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

	shader_set_int_loc(app->billboard_uniforms.irradiance_map, 0);
	shader_set_int_loc(app->billboard_uniforms.prefilter_map, 1);
	shader_set_int_loc(app->billboard_uniforms.brdf_lut, 2);
	shader_set_int_loc(app->billboard_uniforms.debug_mode,
	                   app->pbr_debug_mode);
	shader_set_vec3_loc(app->billboard_uniforms.cam_pos, camera_pos);
	shader_set_mat4_loc(app->billboard_uniforms.projection, (float*)proj);
	shader_set_mat4_loc(app->billboard_uniforms.view, (float*)view);
	shader_set_mat4_loc(
	    app->billboard_uniforms.previous_view_proj,
	    (float*)app->postprocess.motion_blur_fx.previous_view_proj);

	float screen_size[2] = {(float)app->width, (float)app->height};
	shader_set_vec2_loc(app->billboard_uniforms.u_screen_size, screen_size);

	/* Debug Visualization Constants */
	const float debug_fill_alpha = 0.10F;
	const float debug_box_alpha = 0.5F;
	const float debug_offset_fill = 1.0F;
	const float debug_offset_line = -2.0F;

	billboard_group_draw(&app->billboard_group);

	if (app->pbr_debug_mode == 0 && app->wireframe) {
		/* Wireframe Overlay */
		/* Enable Depth Test but disable Depth Write to overlay
		 * correctly */
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		shader_use(app->debug_line_shader);
		shader_set_mat4(app->debug_line_shader, "projection",
		                (float*)proj);
		shader_set_mat4(app->debug_line_shader, "view", (float*)view);

		/* 0. Transparent Fill (Instance Albedo) */
		/* Push fill back to avoid z-fighting with outlines */
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(debug_offset_fill, debug_offset_fill);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		/* Disable stipple, Enable Billboard Mode, Enable Instance Color
		 */
		shader_set_int(app->debug_line_shader, "u_stippled", 0);
		shader_set_int(app->debug_line_shader, "u_billboardMode", 1);
		shader_set_int(app->debug_line_shader, "u_useInstanceColor", 1);
		/* Alpha 0.10 for transparency */
		float color_fill[4] = {1.0F, 1.0F, 1.0F, debug_fill_alpha};
		shader_set_vec4(app->debug_line_shader, "u_color", color_fill);
		billboard_group_draw_debug_fill(&app->billboard_group);
		glDisable(GL_BLEND);
		glDisable(GL_POLYGON_OFFSET_FILL);

		/* 1. Quad Outline (Solid Green/White) */
		/* Pull outlines forward */
		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(debug_offset_line, debug_offset_line);

		/* Disable stipple, Enable Billboard Mode, Disable Instance
		 * Color */
		shader_set_int(app->debug_line_shader, "u_stippled", 0);
		shader_set_int(app->debug_line_shader, "u_billboardMode", 1);
		shader_set_int(app->debug_line_shader, "u_useInstanceColor", 0);
		float color_quad[4] = {0.0F, 1.0F, 0.0F, 1.0F};
		shader_set_vec4(app->debug_line_shader, "u_color", color_quad);
		billboard_group_draw_debug_quads(&app->billboard_group);

		/* 2. Bounding Box (Dotted/Stippled Red/Yellow) */
		/* Enable stipple, Disable Billboard Mode */
		shader_set_int(app->debug_line_shader, "u_stippled", 1);
		shader_set_int(app->debug_line_shader, "u_billboardMode", 0);
		float color_box[4] = {1.0F, 1.0F, 0.0F, debug_box_alpha};
		shader_set_vec4(app->debug_line_shader, "u_color", color_box);
		billboard_group_draw_debug_boxes(&app->billboard_group);

		glDisable(GL_POLYGON_OFFSET_LINE);

		/* Restore Depth State */
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
	}
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
