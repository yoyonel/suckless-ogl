#include "app.h"

#include "adaptive_sampler.h"
#include "app_ui.h"
#include "billboard_rendering.h"
#include "gl_common.h"
#include "glad/glad.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "material.h"
#include "perf_mode.h"
#include "postprocess.h"
#include "shader.h"
#include "skybox.h"
#include "ui.h"
#include "window.h"
#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif
#include "async_loader.h"
#include "gpu_profiler.h"
#include "gpu_profiler_ui.h"
#include <stdlib.h>

void app_cleanup(App* app)
{
	icosphere_free(&app->geometry);
	skybox_cleanup(&app->skybox);
#ifdef USE_TRANSPARENT_BILLBOARDS
	if (app->sphere_instances) {
		free(app->sphere_instances);
		app->sphere_instances = NULL;
	}
	sphere_sorter_cleanup(&app->sphere_sorter);
#endif
	instanced_group_cleanup(&app->instanced_group);
	billboard_group_cleanup(&app->billboard_group);
#ifdef USE_SSBO_RENDERING
	ssbo_group_cleanup(&app->ssbo_group);
#endif
	if (app->material_lib) {
		material_free_lib(app->material_lib);
		app->material_lib = NULL;
	}

	shader_destroy(app->pbr_instanced_shader);
	app->pbr_instanced_shader = NULL;
	shader_destroy(app->pbr_billboard_shader);
	app->pbr_billboard_shader = NULL;
	shader_destroy(app->debug_shader);
	app->debug_shader = NULL;
	shader_destroy(app->debug_line_shader);
	app->debug_line_shader = NULL;
#ifdef USE_SSBO_RENDERING
	shader_destroy(app->pbr_ssbo_shader);
	app->pbr_ssbo_shader = NULL;
#endif

	shader_destroy(app->skybox_shader);
	app->skybox_shader = NULL;
	glDeleteProgram(app->shader_spmap);
	app->shader_spmap = 0;
	glDeleteProgram(app->shader_irmap);
	app->shader_irmap = 0;
	glDeleteProgram(app->shader_lum_pass1);
	app->shader_lum_pass1 = 0;
	glDeleteProgram(app->shader_lum_pass2);
	app->shader_lum_pass2 = 0;

	glDeleteVertexArrays(1, &app->sphere_vao);
	app->sphere_vao = 0;
	glDeleteVertexArrays(1, &app->empty_vao);
	app->empty_vao = 0;

	glDeleteBuffers(1, &app->sphere_vbo);
	app->sphere_vbo = 0;
	glDeleteBuffers(1, &app->sphere_nbo);
	app->sphere_nbo = 0;
	glDeleteBuffers(1, &app->sphere_ebo);
	app->sphere_ebo = 0;
	glDeleteBuffers(1, &app->wire_cube_vbo);
	app->wire_cube_vbo = 0;
	glDeleteBuffers(1, &app->wire_quad_vbo);
	app->wire_quad_vbo = 0;
	glDeleteBuffers(1, &app->quad_vbo);
	app->quad_vbo = 0;
	glDeleteBuffers(2, app->lum_ssbo);
	app->lum_ssbo[0] = 0;
	app->lum_ssbo[1] = 0;

	ui_destroy(&app->ui);
	postprocess_cleanup(&app->postprocess);
	adaptive_sampler_cleanup(&app->fps_sampler);

	glDeleteTextures(1, &app->hdr_texture);
	app->hdr_texture = 0;
	glDeleteTextures(1, &app->brdf_lut_tex);
	app->brdf_lut_tex = 0;
	glDeleteTextures(1, &app->spec_prefiltered_tex);
	app->spec_prefiltered_tex = 0;
	glDeleteTextures(1, &app->irradiance_tex);
	app->irradiance_tex = 0;
	glDeleteTextures(1, &app->dummy_black_tex);
	app->dummy_black_tex = 0;
	glDeleteTextures(1, &app->dummy_white_tex);
	app->dummy_white_tex = 0;
	if (app->transition_snapshot_tex) {
		glDeleteTextures(1, &app->transition_snapshot_tex);
		app->transition_snapshot_tex = 0;
	}

	if (app->ibl_ctx.pending_hdr_tex) {
		glDeleteTextures(1, &app->ibl_ctx.pending_hdr_tex);
		app->ibl_ctx.pending_hdr_tex = 0;
	}
	if (app->ibl_ctx.pending_spec_tex) {
		glDeleteTextures(1, &app->ibl_ctx.pending_spec_tex);
		app->ibl_ctx.pending_spec_tex = 0;
	}
	if (app->ibl_ctx.pending_irr_tex) {
		glDeleteTextures(1, &app->ibl_ctx.pending_irr_tex);
		app->ibl_ctx.pending_irr_tex = 0;
	}

	glDeleteBuffers(1, &app->exposure_pbo);
	app->exposure_pbo = 0;
	glDeleteBuffers(1, &app->histogram_pbo);
	app->histogram_pbo = 0;

	async_loader_shutdown();

	if (app->hdr_files) {
		for (int i = 0; i < app->hdr_count; i++) {
			free(app->hdr_files[i]);
		}
		free(app->hdr_files);
		app->hdr_files = NULL;
	}
	if (app->lum_histogram_buffer) {
		free(app->lum_histogram_buffer);
		app->lum_histogram_buffer = NULL;
	}

	perf_mode_cleanup(&app->perf_context);

	gpu_profiler_cleanup(&app->gpu_profiler);
	gpu_profiler_ui_cleanup(&app->timeline_ui);

	if (app->window) {
		window_destroy(app->window);
		app->window = NULL;
	}
}
