#include "app_settings.h"
#include "billboard_rendering.h"
#include "billboard_sorting.h"
#include "gl_debug.h"
#include "glad/glad.h"
#include "gpu_profiler.h"
#include "light_probes.h"
#include "profiler.h"
#include "scene.h"
#include "shader.h"
#include "shockwave.h"
#include "skybox.h"
#include "trail_renderer.h"
#include <string.h>

#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif

const char* aa_mode_to_string(AAMode mode)
{
	switch (mode) {
		case AA_MODE_SCREEN_SPACE:
			return "Screen-space";
		case AA_MODE_CURVATURE:
			return "Curvature-based";
		default:
			return "Unknown";
	}
}

void scene_update_gpu_buffers(Scene* scene)
{
	glBindVertexArray(scene->gpu.icosphere_vao);
	glBindBuffer(GL_ARRAY_BUFFER, scene->gpu.icosphere_vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(scene->geometry.vertices.size * sizeof(vec3)),
	             scene->geometry.vertices.data, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, scene->gpu.icosphere_nbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(scene->geometry.normals.size * sizeof(vec3)),
	             scene->geometry.normals.data, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->gpu.icosphere_ebo);
	glBufferData(
	    GL_ELEMENT_ARRAY_BUFFER,
	    (GLsizeiptr)(scene->geometry.indices.size * sizeof(unsigned int)),
	    scene->geometry.indices.data, GL_STATIC_DRAW);
	glBindVertexArray(0);
}

/**
 * @brief Binds SH 3D textures (units 8-14) and light probe grid SSBO only when
 * changed. Units 8-14 and SSBO binding 3 are exclusive to PBR passes — safe to
 * cache. Invalidated after light_probe_grid_sync() which clobbers
 * GL_TEXTURE_3D.
 */
static void scene_bind_probe_textures(Scene* scene)
{
	for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
		GLuint tex = scene->lighting.probe_grid.sh_textures[i];
		if (tex != scene->gpu.bound_sh_textures[i]) {
			glActiveTexture(
			    (GLenum)(GL_TEXTURE0 + TEXTURE_UNIT_SH_START + i));
			glBindTexture(GL_TEXTURE_3D, tex);
			scene->gpu.bound_sh_textures[i] = tex;
		}
	}

	if (scene->lighting.probe_grid.ssbo != scene->gpu.bound_probe_ssbo) {
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3,
		                 scene->lighting.probe_grid.ssbo);
		scene->gpu.bound_probe_ssbo = scene->lighting.probe_grid.ssbo;
	}
}

static void scene_bind_ibl_textures(Scene* scene)
{
	GLuint textures[IBL_TEXTURE_COUNT] = {
	    scene->gpu.irradiance_tex ? scene->gpu.irradiance_tex
	                              : scene->gpu.dummy_black_tex,
	    scene->gpu.spec_prefiltered_tex ? scene->gpu.spec_prefiltered_tex
	                                    : scene->gpu.dummy_black_tex,
	    scene->gpu.brdf_lut_tex ? scene->gpu.brdf_lut_tex
	                            : scene->gpu.dummy_black_tex};

	for (int i = 0; i < IBL_TEXTURE_COUNT; i++) {
		if (textures[i] != scene->gpu.bound_ibl_textures[i]) {
			glActiveTexture(
			    (GLenum)(GL_TEXTURE0 + TEXTURE_UNIT_IBL_START + i));
			glBindTexture(GL_TEXTURE_2D, textures[i]);
			scene->gpu.bound_ibl_textures[i] = textures[i];
		}
	}
}

static void scene_render_billboards(Scene* scene, mat4 view, mat4 proj,
                                    vec3 camera_pos, mat4 previous_view_proj,
                                    int width, int height)
{
	Shader* current_shader = scene->shaders.pbr_billboard;
	shader_use(current_shader);

	scene_bind_ibl_textures(scene);

	/* Upload all per-frame uniforms via UBO (binding = 1) */
	{
		BillboardUBO ubo = {0};
		glm_mat4_copy(proj, (vec4*)ubo.projection);
		glm_mat4_copy(view, (vec4*)ubo.view);
		glm_mat4_copy(previous_view_proj,
		              (vec4*)ubo.previous_view_proj);
		glm_vec3_copy(camera_pos, ubo.cam_pos);
		ubo.debug_mode = scene->config.pbr_debug_mode;
		ubo.screen_size[0] = (float)width;
		ubo.screen_size[1] = (float)height;
		glm_vec3_copy(scene->lighting.probe_grid.aabb_min,
		              ubo.probe_grid_min);
		ubo.gi_mode = (int32_t)scene->config.gi_mode;
		glm_vec3_copy(scene->lighting.probe_grid.aabb_max,
		              ubo.probe_grid_max);
		ubo.specular_aa_enabled = scene->config.specular_aa_enabled;
		ubo.probe_grid_dim[0] = scene->lighting.probe_grid.grid_dim[0];
		ubo.probe_grid_dim[1] = scene->lighting.probe_grid.grid_dim[1];
		ubo.probe_grid_dim[2] = scene->lighting.probe_grid.grid_dim[2];
		ubo.aa_mode = scene->config.aa_mode;

		if (scene->gpu.billboard_ubo_ptr) {
			*(BillboardUBO*)scene->gpu.billboard_ubo_ptr = ubo;
		}
	}

	scene_bind_probe_textures(scene);

	/* Debug Visualization Constants */
	const float debug_fill_alpha = 0.10F;
	const float debug_box_alpha = 0.5F;
	const float debug_offset_fill = 1.0F;
	const float debug_offset_line = -2.0F;

	billboard_group_draw(&scene->billboard_group);

	if (scene->config.pbr_debug_mode == 0 && scene->config.wireframe) {
		/* Wireframe Overlay */
		/* Enable Depth Test but disable Depth Write to overlay
		 * correctly */
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		shader_use(scene->shaders.debug_line);
		shader_set_mat4_loc(scene->debug_uniforms.projection,
		                    (float*)proj);
		shader_set_mat4_loc(scene->debug_uniforms.view, (float*)view);

		/* 0. Transparent Fill (Instance Albedo) */
		/* Push fill back to avoid z-fighting with outlines */
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(debug_offset_fill, debug_offset_fill);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		/* Disable stipple, Enable Billboard Mode, Enable Instance Color
		 */
		shader_set_int_loc(scene->debug_uniforms.u_stippled, 0);
		shader_set_int_loc(scene->debug_uniforms.u_billboard_mode, 1);
		shader_set_int_loc(scene->debug_uniforms.u_use_instance_col, 1);
		/* Alpha 0.10 for transparency */
		float color_fill[4] = {1.0F, 1.0F, 1.0F, debug_fill_alpha};
		shader_set_vec4_loc(scene->debug_uniforms.u_color, color_fill);
		billboard_group_draw_debug_fill(&scene->billboard_group);
		glDisable(GL_BLEND);
		glDisable(GL_POLYGON_OFFSET_FILL);

		/* 1. Quad Outline (Solid Green/White) */
		/* Pull outlines forward */
		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(debug_offset_line, debug_offset_line);

		/* Disable stipple, Enable Billboard Mode, Disable Instance
		 * Color */
		shader_set_int_loc(scene->debug_uniforms.u_stippled, 0);
		shader_set_int_loc(scene->debug_uniforms.u_billboard_mode, 1);
		shader_set_int_loc(scene->debug_uniforms.u_use_instance_col, 0);
		float color_quad[4] = {0.0F, 1.0F, 0.0F, 1.0F};
		shader_set_vec4_loc(scene->debug_uniforms.u_color, color_quad);
		billboard_group_draw_debug_quads(&scene->billboard_group);

		/* 2. Bounding Box (Dotted/Stippled Red/Yellow) */
		/* Enable stipple, Disable Billboard Mode */
		shader_set_int_loc(scene->debug_uniforms.u_stippled, 1);
		shader_set_int_loc(scene->debug_uniforms.u_billboard_mode, 0);
		float color_box[4] = {1.0F, 1.0F, 0.0F, debug_box_alpha};
		shader_set_vec4_loc(scene->debug_uniforms.u_color, color_box);
		billboard_group_draw_debug_boxes(&scene->billboard_group);

		glDisable(GL_POLYGON_OFFSET_LINE);

		/* Restore Depth State */
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
	}
}

static void scene_render_instanced(Scene* scene, mat4 view, mat4 proj,
                                   vec3 camera_pos, mat4 previous_view_proj)
{
	Shader* current_shader = NULL;
#ifdef USE_SSBO_RENDERING
	current_shader = scene->shaders.pbr_ssbo;
#else
	current_shader = scene->shaders.pbr_instanced;
#endif

	shader_use(current_shader);

	scene_bind_ibl_textures(scene);

	shader_set_int_loc(scene->instanced_uniforms.debug_mode,
	                   scene->config.pbr_debug_mode);
	shader_set_vec3_loc(scene->instanced_uniforms.cam_pos, camera_pos);
	shader_set_mat4_loc(scene->instanced_uniforms.projection, (float*)proj);
	shader_set_mat4_loc(scene->instanced_uniforms.view, (float*)view);
	shader_set_mat4_loc(scene->instanced_uniforms.previous_view_proj,
	                    (float*)previous_view_proj);

	if (scene->instanced_uniforms.u_specular_aa_enabled != -1) {
		glUniform1i(scene->instanced_uniforms.u_specular_aa_enabled,
		            scene->config.specular_aa_enabled);
	}
	if (scene->instanced_uniforms.u_aa_mode != -1) {
		glUniform1i(scene->instanced_uniforms.u_aa_mode,
		            scene->config.aa_mode);
	}

	/* Probe Grid spatial bounds and GI Toggle */
	shader_set_vec3_loc(scene->instanced_uniforms.probe_grid_min,
	                    scene->lighting.probe_grid.aabb_min);
	shader_set_vec3_loc(scene->instanced_uniforms.probe_grid_max,
	                    scene->lighting.probe_grid.aabb_max);

	if (scene->instanced_uniforms.probe_grid_dim != -1) {
		glUniform3i(scene->instanced_uniforms.probe_grid_dim,
		            scene->lighting.probe_grid.grid_dim[0],
		            scene->lighting.probe_grid.grid_dim[1],
		            scene->lighting.probe_grid.grid_dim[2]);
	}
	shader_set_int_loc(scene->instanced_uniforms.gi_mode,
	                   (int)scene->config.gi_mode);

	scene_bind_probe_textures(scene);

#ifdef USE_SSBO_RENDERING
	ssbo_group_draw(&scene->ssbo_group, scene->geometry.indices.size);
#else
	instanced_group_draw(&scene->instanced_group,
	                     (int)scene->geometry.indices.size);
#endif
}

static inline void stencil_begin_object_pass(void)
{
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilFunc(GL_ALWAYS, 1, DEFAULT_STENCIL_MASK);
	glStencilMask(DEFAULT_STENCIL_MASK);
}

void scene_render(Scene* scene, GPUProfiler* profiler, mat4 view, mat4 proj,
                  vec3 camera_pos, mat4 previous_view_proj, int width,
                  int height)
{
	mat4 view_proj;
	mat4 inv_view_proj;
	glm_mat4_mul(proj, view, view_proj);
	glm_mat4_inv(view_proj, inv_view_proj);

	/* GI Probe SSBO sync — must happen before Spheres read it */
	if (scene->config.gi_mode != GI_MODE_OFF ||
	    scene->config.show_probe_grid) {
		PROFILE_ZONE(gi_sync_ctx,
		             "GI Light Probe Grid Sync (buffer upload)");
		light_probe_grid_sync(&scene->lighting.probe_grid);
		/* Sync clobbers 3D texture bindings on the current unit
		 * via glBindTexture(GL_TEXTURE_3D, 0) — invalidate cache
		 * so scene_bind_probe_textures() will re-bind. */
		for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
			scene->gpu.bound_sh_textures[i] = 0;
		}
		PROFILE_ZONE_END(gi_sync_ctx);
	}

#ifdef USE_TRANSPARENT_BILLBOARDS
	if (scene->config.show_envmap) {
		GPU_STAGE_PROFILER(profiler, "Environment",
		                   GPU_PROFILER_ENV_COLOR);
		gl_debug_push_group("Skybox_Pass");
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_DEPTH_TEST);
		skybox_render(&scene->visuals.skybox, scene->shaders.skybox,
		              scene->gpu.hdr_texture,
		              scene->gpu.dummy_black_tex, inv_view_proj,
		              scene->config.env_lod);
		glEnable(GL_DEPTH_TEST);
		gl_debug_pop_group();
	}

	{
		stencil_begin_object_pass();

		if (scene->config.billboard_mode) {
			gl_debug_push_group("Billboard_Sort_And_Render");
			GLuint sorted_ssbo = 0;

			/* 1. Sorting Pass (CPU or GPU) */
			{
				GPU_STAGE_PROFILER(
				    profiler, "Sphere Sort",
				    GPU_PROFILER_MOTION_BLUR_COLOR);

				switch (scene->config.sorting_mode) {
					case SORTING_MODE_CPU_QSORT:
						sorted_ssbo =
						    billboard_sorter_sort_cpu(
						        &scene
						             ->billboard_sorter,
						        scene
						            ->billboard_instances,
						        scene
						            ->billboard_instance_count,
						        camera_pos);
						break;
					case SORTING_MODE_CPU_RADIX:
						sorted_ssbo =
						    billboard_sorter_sort_cpu_radix(
						        &scene
						             ->billboard_sorter,
						        scene
						            ->billboard_instances,
						        scene
						            ->billboard_instance_count,
						        camera_pos);
						break;
					case SORTING_MODE_GPU_BITONIC:
					default:
						sorted_ssbo =
						    billboard_sorter_sort_gpu(
						        &scene
						             ->billboard_sorter,
						        scene
						            ->billboard_instances,
						        scene
						            ->billboard_instance_count,
						        camera_pos);
						break;
				}
			}

			/* Tier 4: Sorted SSBO already bound at binding 2 by
			 * sort functions — vertex shader reads it directly
			 * via gl_InstanceID (no VBO copy needed). */
			scene->billboard_group.instance_count =
			    scene->billboard_instance_count;

			/* Legacy VBO copy only for debug wireframe overlay
			 * (debug_line_shader reads per-SphereInstance
			 * attributes) */
			if (scene->config.wireframe) {
				billboard_group_update_from_buffer(
				    &scene->billboard_group, sorted_ssbo,
				    scene->billboard_instance_count);
			}

			/* 2. Actual Billboard Rendering */
			{
				GPU_STAGE_PROFILER(profiler, "Billboard Render",
				                   GPU_PROFILER_SCENE_COLOR);

				glEnablei(GL_BLEND, 0);
				glBlendFunc(GL_SRC_ALPHA,
				            GL_ONE_MINUS_SRC_ALPHA);
				glDisablei(GL_BLEND, 1);

				scene_render_billboards(
				    scene, view, proj, camera_pos,
				    previous_view_proj, width, height);

				glDisablei(GL_BLEND, 0);
			}

			gl_debug_pop_group();
		} else {
			GPU_STAGE_PROFILER(profiler, "Instanced Render",
			                   GPU_PROFILER_SCENE_COLOR);
			gl_debug_push_group("Instanced_Geometry_Render");
			glPolygonMode(GL_FRONT_AND_BACK, scene->config.wireframe
			                                     ? GL_LINE
			                                     : GL_FILL);

			scene_render_instanced(scene, view, proj, camera_pos,
			                       previous_view_proj);

			if (scene->config.wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			gl_debug_pop_group();
		}

		glDisable(GL_STENCIL_TEST);
	}
#else
	{
		stencil_begin_object_pass();

		if (scene->config.billboard_mode) {
			gl_debug_push_group("Billboard_Render");

			/* 1. Dummy sort (legacy/fallback path) */
			{
				GPU_STAGE_PROFILER(
				    profiler, "Sphere Sort",
				    GPU_PROFILER_MOTION_BLUR_COLOR);
				/* In fallback path, sorting is handled outside
				 * or not at all */
			}

			/* 2. Actual Billboard Rendering */
			{
				GPU_STAGE_PROFILER(profiler, "Billboard Render",
				                   GPU_PROFILER_SCENE_COLOR);
				scene_render_billboards(
				    scene, view, proj, camera_pos,
				    previous_view_proj, width, height);
			}

			gl_debug_pop_group();
		} else {
			GPU_STAGE_PROFILER(profiler, "Instanced Render",
			                   GPU_PROFILER_SCENE_COLOR);
			gl_debug_push_group("Instanced_Geometry_Render");
			glPolygonMode(GL_FRONT_AND_BACK, scene->config.wireframe
			                                     ? GL_LINE
			                                     : GL_FILL);
			scene_render_instanced(scene, view, proj, camera_pos,
			                       previous_view_proj);

			if (scene->config.wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			gl_debug_pop_group();
		}

		glDisable(GL_STENCIL_TEST);
	}

#endif

	/* --- N-Body orbital trails (rendered after spheres, into HDR FBO) ---
	 */
	if (scene->simulation.nbody_mode) {
		GPU_STAGE_PROFILER(profiler, "NBody Trails",
		                   GPU_PROFILER_NBODY_COLOR);
		gl_debug_push_group("NBody_Trails");
		trail_renderer_draw(&scene->visuals.trail_renderer, view, proj,
		                    camera_pos);
		gl_debug_pop_group();

		/* Confinement shockwave VFX — billboard lensing, after trails
		 */
		{
			GPU_STAGE_PROFILER(profiler, "Shockwave VFX",
			                   GPU_PROFILER_SHOCKWAVE_COLOR);
			gl_debug_push_group("Shockwave_VFX");
			if (scene->config.wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			}
			shockwave_draw(&scene->visuals.shockwave_renderer, view,
			               proj, camera_pos,
			               scene->simulation.nbody_sim.sim_time,
			               width, height);
			if (scene->config.wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			gl_debug_pop_group();
		}
	}

	if (scene->config.show_probe_grid) {
		light_probe_grid_render_debug(&scene->lighting.probe_grid, view,
		                              proj);
	}
}
