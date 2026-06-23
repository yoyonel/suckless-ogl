#include "billboard_rendering.h"
#include "billboard_sorting.h"
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

static void scene_cleanup_pbr_shaders(Scene* scene)
{
	SHADER_SAFE_DESTROY(scene->shaders->pbr_instanced);
	SHADER_SAFE_DESTROY(scene->shaders->pbr_billboard);
#ifdef USE_SSBO_RENDERING
	SHADER_SAFE_DESTROY(scene->shaders->pbr_ssbo);
#endif
	GL_SAFE_DELETE_PROGRAM(scene->gpu->spmap_program);
	GL_SAFE_DELETE_PROGRAM(scene->gpu->irmap_program);
}

static void scene_cleanup_shaders(Scene* scene)
{
	scene_cleanup_pbr_shaders(scene);

	SHADER_SAFE_DESTROY(scene->shaders->debug);
	SHADER_SAFE_DESTROY(scene->shaders->debug_line);
	SHADER_SAFE_DESTROY(scene->shaders->skybox);
	GL_SAFE_DELETE_PROGRAM(scene->gpu->lum_pass1_program);
	GL_SAFE_DELETE_PROGRAM(scene->gpu->lum_pass2_program);
}

static void scene_cleanup_geometry_buffers(Scene* scene)
{
	GL_SAFE_DELETE_VAO(scene->gpu->icosphere_vao);
	GL_SAFE_DELETE_BUFFER(scene->gpu->icosphere_vbo);
	GL_SAFE_DELETE_BUFFER(scene->gpu->icosphere_nbo);
	GL_SAFE_DELETE_BUFFER(scene->gpu->icosphere_ebo);
}

static void scene_cleanup_buffers(Scene* scene)
{
	scene_cleanup_geometry_buffers(scene);

	GL_SAFE_DELETE_VAO(scene->gpu->empty_vao);
	GL_SAFE_DELETE_BUFFER(scene->gpu->wire_cube_vbo);
	GL_SAFE_DELETE_BUFFER(scene->gpu->wire_quad_vbo);
	GL_SAFE_DELETE_BUFFER(scene->gpu->quad_vbo);
	GL_SAFE_DELETE_BUFFERS(2, scene->gpu->lum_ssbo);
	GL_SAFE_DELETE_BUFFER(scene->gpu->billboard_ubo);
}

static void scene_cleanup_textures(Scene* scene)
{
	GL_SAFE_DELETE_TEXTURE(scene->gpu->hdr_texture);
	GL_SAFE_DELETE_TEXTURE(scene->gpu->recycled_hdr_tex);
	GL_SAFE_DELETE_TEXTURE(scene->gpu->brdf_lut_tex);
	GL_SAFE_DELETE_TEXTURE(scene->gpu->spec_prefiltered_tex);
	GL_SAFE_DELETE_TEXTURE(scene->gpu->irradiance_tex);
	GL_SAFE_DELETE_TEXTURE(scene->gpu->dummy_black_tex);
	GL_SAFE_DELETE_TEXTURE(scene->gpu->dummy_white_tex);
	GL_SAFE_DELETE_TEXTURE(scene->gpu->transition_snapshot_tex);
}

static void scene_cleanup_gpu_resources(Scene* scene)
{
	scene_cleanup_buffers(scene);
	scene_cleanup_textures(scene);
}

void scene_cleanup(Scene* scene)
{
	if (!scene) {
		return;
	}

	icosphere_free(&scene->geometry);
	skybox_cleanup(&scene->visuals->skybox);
#ifdef USE_TRANSPARENT_BILLBOARDS
	if (scene->billboard_instances) {
		platform_aligned_free(scene->billboard_instances);
		scene->billboard_instances = NULL;
	}
	billboard_sorter_cleanup(&scene->billboard_sorter);
#endif
	instanced_group_cleanup(&scene->instanced_group);
	billboard_group_cleanup(&scene->billboard_group);
	trail_renderer_cleanup(&scene->visuals->trail_renderer);
	shockwave_renderer_cleanup(&scene->visuals->shockwave_renderer);
#ifdef USE_SSBO_RENDERING
	ssbo_group_cleanup(&scene->ssbo_group);
#endif

	if (scene->lighting.material_lib) {
		material_free_lib(scene->lighting.material_lib);
		scene->lighting.material_lib = NULL;
	}

	scene_cleanup_shaders(scene);
	scene_cleanup_gpu_resources(scene);

	ibl_coordinator_cleanup(&scene->lighting.ibl_coord);
	light_probe_grid_cleanup(&scene->lighting.probe_grid);

	if (scene->hdr_files) {
		for (int i = 0; i < scene->hdr_count; i++) {
			free(scene->hdr_files[i]);
			scene->hdr_files[i] = NULL;
		}
		free(scene->hdr_files);
		scene->hdr_files = NULL;
		scene->hdr_count = 0;
	}

	/* Free opaque sub-structs */
	free(scene->gpu);
	scene->gpu = NULL;
	free(scene->shaders);
	scene->shaders = NULL;
	free(scene->simulation);
	scene->simulation = NULL;
	free(scene->visuals);
	scene->visuals = NULL;
}
