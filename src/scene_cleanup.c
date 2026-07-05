#include "billboard_renderer.h"
#include "gl_common.h"
#include "ibl_coordinator.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "light_probes.h"
#include "material.h"
#include "platform/platform_utils.h"
#include "scene.h"
#include "scene_gpu_resources.h"
#include "scene_shaders.h"
#include "scene_visuals.h"
#include "shader.h"
#include "shockwave.h"
#include "skybox.h"
#include "trail_renderer.h"
#include <stdlib.h>
#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif

static void safe_delete_texture(GLuint* tex)
{
	if (tex) {
		GL_SAFE_DELETE_TEXTURE(*tex);
	}
}

static void safe_delete_buffer(GLuint* buf)
{
	if (buf) {
		GL_SAFE_DELETE_BUFFER(*buf);
	}
}

static void safe_delete_vao(GLuint* vao)
{
	if (vao) {
		GL_SAFE_DELETE_VAO(*vao);
	}
}

static void safe_delete_program(GLuint* prog)
{
	if (prog) {
		GL_SAFE_DELETE_PROGRAM(*prog);
	}
}

static void safe_destroy_shader(Shader** shader)
{
	if (shader && *shader) {
		SHADER_SAFE_DESTROY(*shader);
	}
}

static void scene_cleanup_pbr_shaders(Scene* scene)
{
	if (!scene || !scene->shaders) {
		goto cleanup;
	}
	safe_destroy_shader(&scene->shaders->pbr_instanced);
	safe_destroy_shader(&scene->shaders->pbr_billboard);
#ifdef USE_SSBO_RENDERING
	safe_destroy_shader(&scene->shaders->pbr_ssbo);
#endif
cleanup:
	return;
}

static void scene_cleanup_gpu_programs(Scene* scene)
{
	if (!scene || !scene->gpu) {
		goto cleanup;
	}
	safe_delete_program(&scene->gpu->spmap_program);
	safe_delete_program(&scene->gpu->irmap_program);
	safe_delete_program(&scene->gpu->lum_pass1_program);
	safe_delete_program(&scene->gpu->lum_pass2_program);
cleanup:
	return;
}

static void scene_cleanup_shaders(Scene* scene)
{
	if (!scene) {
		goto cleanup;
	}
	scene_cleanup_pbr_shaders(scene);
	scene_cleanup_gpu_programs(scene);

	if (scene->shaders) {
		safe_destroy_shader(&scene->shaders->debug);
		safe_destroy_shader(&scene->shaders->debug_line);
		safe_destroy_shader(&scene->shaders->skybox);
	}
cleanup:
	return;
}

static void scene_cleanup_geometry_buffers(Scene* scene)
{
	if (!scene || !scene->gpu) {
		goto cleanup;
	}
	safe_delete_vao(&scene->gpu->icosphere_vao);
	safe_delete_buffer(&scene->gpu->icosphere_vbo);
	safe_delete_buffer(&scene->gpu->icosphere_nbo);
	safe_delete_buffer(&scene->gpu->icosphere_ebo);
cleanup:
	return;
}

static void scene_cleanup_buffers(Scene* scene)
{
	if (!scene || !scene->gpu) {
		goto cleanup;
	}
	scene_cleanup_geometry_buffers(scene);

	safe_delete_vao(&scene->gpu->empty_vao);
	safe_delete_buffer(&scene->gpu->wire_cube_vbo);
	safe_delete_buffer(&scene->gpu->wire_quad_vbo);
	safe_delete_buffer(&scene->gpu->quad_vbo);
	GL_SAFE_DELETE_BUFFERS(2, scene->gpu->lum_ssbo);
	safe_delete_buffer(&scene->gpu->billboard_ubo);
cleanup:
	return;
}

static void scene_cleanup_textures(Scene* scene)
{
	if (!scene || !scene->gpu) {
		goto cleanup;
	}
	safe_delete_texture(&scene->gpu->hdr_texture);
	safe_delete_texture(&scene->gpu->recycled_hdr_tex);
	safe_delete_texture(&scene->gpu->brdf_lut_tex);
	safe_delete_texture(&scene->gpu->spec_prefiltered_tex);
	safe_delete_texture(&scene->gpu->irradiance_tex);
	safe_delete_texture(&scene->gpu->dummy_black_tex);
	safe_delete_texture(&scene->gpu->dummy_white_tex);
	safe_delete_texture(&scene->gpu->transition_snapshot_tex);
cleanup:
	return;
}

static void scene_cleanup_gpu_resources(Scene* scene)
{
	if (!scene) {
		goto cleanup;
	}
	scene_cleanup_buffers(scene);
	scene_cleanup_textures(scene);
cleanup:
	return;
}

void scene_cleanup(Scene* scene)
{
	if (!scene) {
		goto cleanup;
	}

	/* 1. Components & Renderers (Highest dependencies) */
	if (scene->visuals) {
		skybox_cleanup(&scene->visuals->skybox);
		trail_renderer_cleanup(&scene->visuals->trail_renderer);
		shockwave_renderer_cleanup(&scene->visuals->shockwave_renderer);
	}

	if (scene->gpu) {
		billboard_renderer_cleanup(&scene->billboard_renderer);
		instanced_group_cleanup(&scene->instanced_group);
#ifdef USE_SSBO_RENDERING
		ssbo_group_cleanup(&scene->ssbo_group);
#endif
	}

#ifdef USE_TRANSPARENT_BILLBOARDS
	if (scene->billboard_instances) {
		platform_aligned_free(scene->billboard_instances);
		scene->billboard_instances = NULL;
	}
#endif

	icosphere_free(&scene->geometry);

	/* 2. Lighting & Env Resources */
	if (scene->lighting.material_lib) {
		material_free_lib(scene->lighting.material_lib);
		scene->lighting.material_lib = NULL;
	}

	if (scene->gpu) {
		ibl_coordinator_cleanup(&scene->lighting.ibl_coord);
		light_probe_grid_cleanup(&scene->lighting.probe_grid);
	}

	if (scene->hdr_files) {
		for (int i = 0; i < scene->hdr_count; i++) {
			if (scene->hdr_files[i]) {
				free(scene->hdr_files[i]);
				scene->hdr_files[i] = NULL;
			}
		}
		free(scene->hdr_files);
		scene->hdr_files = NULL;
		scene->hdr_count = 0;
	}

	/* 3. Shaders and GPU low-level resources */
	scene_cleanup_shaders(scene);
	scene_cleanup_gpu_resources(scene);

	/* 4. Structural opaque containers (Least dependent) */
	if (scene->gpu) {
		free(scene->gpu);
		scene->gpu = NULL;
	}
	if (scene->shaders) {
		free(scene->shaders);
		scene->shaders = NULL;
	}
	if (scene->simulation) {
		free(scene->simulation);
		scene->simulation = NULL;
	}
	if (scene->visuals) {
		free(scene->visuals);
		scene->visuals = NULL;
	}

cleanup:
	return;
}
