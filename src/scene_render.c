#include "app_settings.h"
#include "billboard_renderer.h"
#include "bool_utils.h"
#include "gl_debug.h"
#include "glad/glad.h"
#include "gpu_profiler.h"
#include "instanced_rendering.h"  // IWYU pragma: export
#include "light_probes.h"
#include "profiler.h"
#include "scene.h"
#include "scene_config.h"
#include "scene_gpu_resources.h"
#include "scene_shaders.h"
#include "scene_simulation.h"
#include "scene_uniforms.h"
#include "scene_visuals.h"
#include "shader.h"
#include "shockwave.h"
#include "skybox.h"
#include "trail_renderer.h"
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/vec3.h>
#include <stdint.h>
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
	glBindVertexArray(scene->gpu->icosphere_vao);

	glBindBuffer(GL_ARRAY_BUFFER, scene->gpu->icosphere_vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(scene->geometry.vertices.size * sizeof(vec3)),
	             scene->geometry.vertices.data, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, scene->gpu->icosphere_nbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(scene->geometry.normals.size * sizeof(vec3)),
	             scene->geometry.normals.data, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexAttribBinding(0, 0);
	glBindVertexBuffer(0, scene->gpu->icosphere_vbo, 0,
	                   (GLsizei)sizeof(vec3));

	glEnableVertexAttribArray(1);
	glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexAttribBinding(1, 1);
	glBindVertexBuffer(1, scene->gpu->icosphere_nbo, 0,
	                   (GLsizei)sizeof(vec3));

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->gpu->icosphere_ebo);
	glBufferData(
	    GL_ELEMENT_ARRAY_BUFFER,
	    (GLsizeiptr)(scene->geometry.indices.size * sizeof(unsigned int)),
	    scene->geometry.indices.data, GL_STATIC_DRAW);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
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
		if (tex != scene->gpu->bound_sh_textures[i]) {
			glActiveTexture(
			    (GLenum)(GL_TEXTURE0 + TEXTURE_UNIT_SH_START + i));
			glBindTexture(GL_TEXTURE_3D, tex);
			scene->gpu->bound_sh_textures[i] = tex;
		}
	}

	if (scene->lighting.probe_grid.ssbo != scene->gpu->bound_probe_ssbo) {
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3,
		                 scene->lighting.probe_grid.ssbo);
		scene->gpu->bound_probe_ssbo = scene->lighting.probe_grid.ssbo;
	}
}

static void scene_bind_ibl_textures(Scene* scene)
{
	GLuint textures[IBL_TEXTURE_COUNT] = {
	    scene->gpu->irradiance_tex ? scene->gpu->irradiance_tex
	                               : scene->gpu->dummy_black_tex,
	    scene->gpu->spec_prefiltered_tex ? scene->gpu->spec_prefiltered_tex
	                                     : scene->gpu->dummy_black_tex,
	    scene->gpu->brdf_lut_tex ? scene->gpu->brdf_lut_tex
	                             : scene->gpu->dummy_black_tex};

	for (int i = 0; i < IBL_TEXTURE_COUNT; i++) {
		if (textures[i] != scene->gpu->bound_ibl_textures[i]) {
			glActiveTexture(
			    (GLenum)(GL_TEXTURE0 + TEXTURE_UNIT_IBL_START + i));
			glBindTexture(GL_TEXTURE_2D, textures[i]);
			scene->gpu->bound_ibl_textures[i] = textures[i];
		}
	}
}

static void scene_render_instanced(Scene* scene, mat4 view, mat4 proj,
                                   vec3 camera_pos, mat4 previous_view_proj)
{
	Shader* current_shader = NULL;
#ifdef USE_SSBO_RENDERING
	current_shader = scene->shaders->pbr_ssbo;
#else
	current_shader = scene->shaders->pbr_instanced;
#endif

	shader_use(current_shader);

	scene_bind_ibl_textures(scene);

	shader_set_int_loc(scene->shaders->instanced_uniforms.debug_mode,
	                   scene->config.pbr_debug_mode);
	shader_set_vec3_loc(scene->shaders->instanced_uniforms.cam_pos,
	                    camera_pos);
	shader_set_mat4_loc(scene->shaders->instanced_uniforms.projection,
	                    (float*)proj);
	shader_set_mat4_loc(scene->shaders->instanced_uniforms.view,
	                    (float*)view);
	shader_set_mat4_loc(
	    scene->shaders->instanced_uniforms.previous_view_proj,
	    (float*)previous_view_proj);

	if (scene->shaders->instanced_uniforms.u_specular_aa_enabled != -1) {
		glUniform1i(
		    scene->shaders->instanced_uniforms.u_specular_aa_enabled,
		    BOOL_TO_INT(scene->config.specular_aa_enabled));
	}
	if (scene->shaders->instanced_uniforms.u_aa_mode != -1) {
		glUniform1i(scene->shaders->instanced_uniforms.u_aa_mode,
		            scene->config.aa_mode);
	}

	/* Probe Grid spatial bounds and GI Toggle */
	shader_set_vec3_loc(scene->shaders->instanced_uniforms.probe_grid_min,
	                    scene->lighting.probe_grid.aabb_min);
	shader_set_vec3_loc(scene->shaders->instanced_uniforms.probe_grid_max,
	                    scene->lighting.probe_grid.aabb_max);

	if (scene->shaders->instanced_uniforms.probe_grid_dim != -1) {
		glUniform3i(scene->shaders->instanced_uniforms.probe_grid_dim,
		            scene->lighting.probe_grid.grid_dim[0],
		            scene->lighting.probe_grid.grid_dim[1],
		            scene->lighting.probe_grid.grid_dim[2]);
	}
	shader_set_int_loc(scene->shaders->instanced_uniforms.gi_mode,
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

static void scene_render_skybox_pass(Scene* scene, GPUProfiler* profiler,
                                     mat4 inv_view_proj)
{
#ifdef USE_TRANSPARENT_BILLBOARDS
	if (scene->config.show_envmap) {
		GPU_STAGE_PROFILER(profiler, "Environment",
		                   GPU_PROFILER_ENV_COLOR);
		gl_debug_push_group("Skybox_Pass");
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_DEPTH_TEST);
		skybox_render(&scene->visuals->skybox, scene->shaders->skybox,
		              scene->gpu->hdr_texture,
		              scene->gpu->dummy_black_tex, inv_view_proj,
		              scene->config.env_lod);
		glEnable(GL_DEPTH_TEST);
		gl_debug_pop_group();
	}
#else
	(void)scene;
	(void)profiler;
	(void)inv_view_proj;
#endif
}

static void scene_render_billboards_pass(Scene* scene, GPUProfiler* profiler,
                                         mat4 view, mat4 proj,
                                         mat4 previous_view_proj,
                                         vec3 camera_pos, int width, int height)
{
#ifdef USE_TRANSPARENT_BILLBOARDS
	gl_debug_push_group("Billboard_Sort_And_Render");
	GPU_STAGE_PROFILER(profiler, "Billboard Render",
	                   GPU_PROFILER_SCENE_COLOR);

	glEnablei(GL_BLEND, 0);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisablei(GL_BLEND, 1);

	/* Bind textures first */
	scene_bind_ibl_textures(scene);
	scene_bind_probe_textures(scene);

	/* Populate params */
	BillboardRenderParams params = {0};
	glm_mat4_copy(proj, params.projection);
	glm_mat4_copy(view, params.view);
	glm_mat4_copy(previous_view_proj, params.previous_view_proj);
	glm_vec3_copy(camera_pos, params.camera_pos);
	params.pbr_debug_mode = scene->config.pbr_debug_mode;
	params.screen_size[0] = (float)width;
	params.screen_size[1] = (float)height;
	glm_vec3_copy(scene->lighting.probe_grid.aabb_min,
	              params.probe_grid_min);
	glm_vec3_copy(scene->lighting.probe_grid.aabb_max,
	              params.probe_grid_max);
	params.probe_grid_dim[0] = scene->lighting.probe_grid.grid_dim[0];
	params.probe_grid_dim[1] = scene->lighting.probe_grid.grid_dim[1];
	params.probe_grid_dim[2] = scene->lighting.probe_grid.grid_dim[2];
	params.gi_mode = (int32_t)scene->config.gi_mode;
	params.specular_aa_enabled = scene->config.specular_aa_enabled;
	params.aa_mode = scene->config.aa_mode;
	params.wireframe = scene->config.wireframe;
	params.sorting_mode = scene->config.sorting_mode;
	params.instances = scene->billboard_instances;
	params.instance_count = scene->billboard_instance_count;

	billboard_renderer_draw(&scene->billboard_renderer, &params,
	                        scene->shaders->pbr_billboard->program,
	                        scene->shaders->debug_line->program,
	                        scene->gpu->billboard_ubo_ptr);

	glDisablei(GL_BLEND, 0);
	gl_debug_pop_group();
#else
	gl_debug_push_group("Billboard_Render");
	/* 1. Dummy sort (legacy/fallback path) */
	{
		GPU_STAGE_PROFILER(profiler, "Sphere Sort",
		                   GPU_PROFILER_MOTION_BLUR_COLOR);
	}
	/* 2. Actual Billboard Rendering */
	{
		GPU_STAGE_PROFILER(profiler, "Billboard Render",
		                   GPU_PROFILER_SCENE_COLOR);
		scene_render_billboards(scene, view, proj, camera_pos,
		                        previous_view_proj, width, height);
	}
	gl_debug_pop_group();
#endif
}

static void scene_render_vfx_pass(Scene* scene, GPUProfiler* profiler,
                                  mat4 view, mat4 proj, vec3 camera_pos,
                                  int width, int height)
{
	if (scene->simulation->nbody_mode) {
		GPU_STAGE_PROFILER(profiler, "NBody Trails",
		                   GPU_PROFILER_NBODY_COLOR);
		gl_debug_push_group("NBody_Trails");
		trail_renderer_draw(&scene->visuals->trail_renderer, view, proj,
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
			shockwave_draw(&scene->visuals->shockwave_renderer,
			               view, proj, camera_pos,
			               scene->simulation->nbody_sim.sim_time,
			               width, height);
			if (scene->config.wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			gl_debug_pop_group();
		}
	}
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
			scene->gpu->bound_sh_textures[i] = 0;
		}
		PROFILE_ZONE_END(gi_sync_ctx);
	}

	scene_render_skybox_pass(scene, profiler, inv_view_proj);

	{
		stencil_begin_object_pass();

		if (scene->config.billboard_mode) {
			scene_render_billboards_pass(scene, profiler, view,
			                             proj, previous_view_proj,
			                             camera_pos, width, height);
		} else {
			GPU_STAGE_PROFILER(profiler, "Instanced Render",
			                   GPU_PROFILER_SCENE_COLOR);
			gl_debug_push_group("Instanced_Geometry_Render");
			glPolygonMode(GL_FRONT_AND_BACK,
			              BOOL_TO_INT(scene->config.wireframe)
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

	/* --- N-Body orbital trails (rendered after spheres, into HDR FBO) ---
	 */
	scene_render_vfx_pass(scene, profiler, view, proj, camera_pos, width,
	                      height);

	if (scene->config.show_probe_grid) {
		light_probe_grid_render_debug(&scene->lighting.probe_grid, view,
		                              proj);
	}
}
