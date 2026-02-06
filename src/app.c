#include "app.h"

#include "action_notifier.h"
#include "adaptive_sampler.h"
#include "app_env.h"
#include "app_input.h"
#include "app_metrics.h"
#include "app_scene.h"
#include "app_settings.h"
#include "app_ui.h"
#include "billboard_rendering.h"
#include "fps.h"
#include "gl_common.h"
#include "glad/glad.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "render_utils.h"
#include "sphere_sorting.h"
#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/util.h>
#include <stdlib.h>
#include <string.h>
#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif
#include "async_loader.h"
#include "camera.h"
#include "material.h"
#include "pbr.h"
#include "perf_mode.h"
#include "postprocess.h"
#include "shader.h"
#include "skybox.h"
#include "ui.h"
#include "window.h"
#include <GLFW/glfw3.h>

int app_init(App* app, int width, int height, const char* title)
{
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	(void)memset(
	    app, 0,
	    sizeof(
	        App));  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

	app->width = width;
	app->height = height;
	app->subdivisions = INITIAL_SUBDIVISIONS;
	app->wireframe = 0;

	app->camera_enabled = 1;
	app->env_lod = DEFAULT_ENV_LOD;
	app->show_info_overlay = 1;
	app->show_exposure_debug = 0;
	app->text_overlay_mode = 0;
	app->pbr_debug_mode = 0;
	app->is_fullscreen = 0;
	app->show_help = 0;
	app->show_envmap = 1;
	app->billboard_mode = 1;
	app->first_mouse = 1;
	app->last_mouse_x = 0.0;
	app->last_mouse_y = 0.0;

	camera_init(&app->camera, DEFAULT_CAMERA_DISTANCE, DEFAULT_CAMERA_YAW,
	            DEFAULT_CAMERA_PITCH);

	app->window = window_create(width, height, title, DEFAULT_SAMPLES);
	if (!app->window) {
		return 0;
	}

	glfwSwapInterval(0);
	glfwSetWindowUserPointer(app->window, app);
	glfwSetKeyCallback(app->window, key_callback);
	glfwSetCursorPosCallback(app->window, mouse_callback);
	glfwSetScrollCallback(app->window, scroll_callback);
	glfwSetFramebufferSizeCallback(app->window, framebuffer_size_callback);

	if (app->camera_enabled) {
		glfwSetInputMode(app->window, GLFW_CURSOR,
		                 GLFW_CURSOR_DISABLED);
	}

	glGenBuffers(1, &app->exposure_pbo);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, app->exposure_pbo);
	glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(float), NULL, GL_STREAM_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	/* Initialize Histogram PBO (64x64 floats) */
	glGenBuffers(1, &app->histogram_pbo);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, app->histogram_pbo);
	glBufferData(GL_PIXEL_PACK_BUFFER,
	             (GLsizeiptr)(LUM_HISTOGRAM_SIZE * sizeof(float)), NULL,
	             GL_STREAM_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	app->current_exposure = 1.0F;
	app->dummy_black_tex = render_utils_create_color_texture(0, 0, 0, 0);
	app->dummy_white_tex = render_utils_create_color_texture(1, 1, 1, 1);

	app->lum_histogram_buffer =
	    malloc((size_t)(LUM_HISTOGRAM_MAP_SIZE * LUM_HISTOGRAM_MAP_SIZE) *
	           sizeof(float));
	if (!app->lum_histogram_buffer) {
		return 0;
	}

	app->brdf_lut_tex = build_brdf_lut_map(BRDF_LUT_MAP_SIZE);
	async_loader_init();

	app_scan_hdr_files(app);
	if (app->hdr_count > 0) {
		int default_idx = 0;
		for (int i = 0; i < app->hdr_count; i++) {
			if (strcmp(app->hdr_files[i], "env.hdr") == 0) {
				default_idx = i;
				break;
			}
		}
		app->current_hdr_index = default_idx;
		app_load_env_map(app, app->hdr_files[default_idx]);
	}

	app->u_metallic = DEFAULT_METALLIC;
	app->u_roughness = DEFAULT_ROUGHNESS;
	app->u_ao = DEFAULT_AO;
	app->u_exposure = DEFAULT_EXPOSURE;

	app->skybox_shader = shader_load_program("shaders/background.vert",
	                                         "shaders/background.frag");
	if (!app->skybox_shader) {
		return 0;
	}

	app->debug_shader =
	    shader_load("shaders/debug_tex.vert", "shaders/debug_tex.frag");
	if (!app->debug_shader) {
		return 0;
	}

	app->debug_line_shader =
	    shader_load("shaders/debug_line.vert", "shaders/debug_line.frag");
	if (!app->debug_line_shader) {
		return 0;
	}

	render_utils_create_empty_vao(&app->empty_vao);

	app->pbr_billboard_shader = shader_load(
	    "shaders/pbr_ibl_billboard.vert", "shaders/pbr_ibl_billboard.frag");
	if (!app->pbr_billboard_shader) {
		return 0;
	}

	render_utils_create_quad_vbo(&app->quad_vbo);
	render_utils_create_wire_cube_vbo(&app->wire_cube_vbo);
	render_utils_create_wire_quad_vbo(&app->wire_quad_vbo);
	skybox_init(&app->skybox, app->skybox_shader);
	icosphere_init(&app->geometry);

	glGenVertexArrays(1, &app->sphere_vao);
	glGenBuffers(1, &app->sphere_vbo);
	glGenBuffers(1, &app->sphere_nbo);
	glGenBuffers(1, &app->sphere_ebo);

	glEnable(GL_DEPTH_TEST);

	fps_init(&app->fps_counter, DEFAULT_FPS_SMOOTHING, DEFAULT_FPS_WINDOW);
	adaptive_sampler_init(&app->fps_sampler, DEFAULT_FPS_WINDOW,
	                      DEFAULT_FPS_SAMPLER_SIZE, DEFAULT_FPS_TARGET);
	app->last_frame_time = glfwGetTime();

	ui_init(&app->ui, "assets/fonts/FiraCode-Regular.ttf",
	        DEFAULT_FONT_SIZE);
	app->material_lib =
	    material_load_presets("assets/materials/pbr_materials.json");

	app->shader_spmap = shader_load_compute("shaders/IBL/spmap.glsl");
	app->shader_irmap = shader_load_compute("shaders/IBL/irmap.glsl");
	app->shader_lum_pass1 =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass1.glsl");
	app->shader_lum_pass2 =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass2.glsl");

#ifdef USE_SSBO_RENDERING
	app_init_ssbo(app);
	app->pbr_ssbo_shader = shader_load("shaders/pbr_ibl_ssbo.vert",
	                                   "shaders/pbr_ibl_instanced.frag");
	if (!app->pbr_ssbo_shader) {
		return 0;
	}
#else
	app_init_instancing(app);
	app->pbr_instanced_shader = shader_load(
	    "shaders/pbr_ibl_instanced.vert", "shaders/pbr_ibl_instanced.frag");
	if (!app->pbr_instanced_shader) {
		return 0;
	}
	app_update_instancing_mode(app);
#endif

	if (!postprocess_init(&app->postprocess, &app->gpu_profiler, width,
	                      height)) {
		return 0;
	}
	postprocess_set_dummy_textures(&app->postprocess, app->dummy_black_tex);
	postprocess_set_exposure(&app->postprocess, app->auto_threshold);
	postprocess_enable(&app->postprocess, POSTFX_FXAA);

#ifdef ENABLE_SHADER_OPTIMIZATION
	postprocess_compile_optimized(&app->postprocess,
	                              app->postprocess.active_effects);
#endif

	perf_mode_init(&app->perf_context);
	action_notifier_init(&app->notifier);

	gpu_profiler_init(&app->gpu_profiler);

	return 1;
}

void app_cleanup(App* app)
{
	icosphere_free(&app->geometry);
	skybox_cleanup(&app->skybox);
#ifdef USE_TRANSPARENT_BILLBOARDS
	if (app->sphere_instances) {
		free(app->sphere_instances);
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
	}

	shader_destroy(app->pbr_instanced_shader);
	shader_destroy(app->pbr_billboard_shader);
	shader_destroy(app->debug_shader);
	shader_destroy(app->debug_line_shader);
#ifdef USE_SSBO_RENDERING
	shader_destroy(app->pbr_ssbo_shader);
#endif

	glDeleteProgram(app->skybox_shader);
	glDeleteProgram(app->shader_spmap);
	glDeleteProgram(app->shader_irmap);
	glDeleteProgram(app->shader_lum_pass1);
	glDeleteProgram(app->shader_lum_pass2);

	glDeleteVertexArrays(1, &app->sphere_vao);
	glDeleteVertexArrays(1, &app->empty_vao);
	glDeleteBuffers(1, &app->sphere_vbo);
	glDeleteBuffers(1, &app->sphere_nbo);
	glDeleteBuffers(1, &app->sphere_ebo);
	glDeleteBuffers(1, &app->wire_cube_vbo);
	glDeleteBuffers(1, &app->wire_quad_vbo);
	glDeleteBuffers(1, &app->quad_vbo);
	glDeleteBuffers(2, app->lum_ssbo);

	ui_destroy(&app->ui);
	postprocess_cleanup(&app->postprocess);
	adaptive_sampler_cleanup(&app->fps_sampler);

	glDeleteTextures(1, &app->hdr_texture);
	glDeleteTextures(1, &app->brdf_lut_tex);
	glDeleteTextures(1, &app->spec_prefiltered_tex);
	glDeleteTextures(1, &app->irradiance_tex);
	glDeleteTextures(1, &app->dummy_black_tex);
	glDeleteTextures(1, &app->dummy_white_tex);
	glDeleteBuffers(1, &app->exposure_pbo);
	glDeleteBuffers(1, &app->histogram_pbo);

	async_loader_shutdown();

	if (app->hdr_files) {
		for (int i = 0; i < app->hdr_count; i++) {
			free(app->hdr_files[i]);
		}
		free(app->hdr_files);
	}
	if (app->lum_histogram_buffer) {
		free(app->lum_histogram_buffer);
	}

	perf_mode_cleanup(&app->perf_context);

	gpu_profiler_cleanup(&app->gpu_profiler);

	window_destroy(app->window);
}

void app_run(App* app)
{
	int last_subdiv = -1;
	while (!glfwWindowShouldClose(app->window)) {
		app->frame_count++;
		double current_time = glfwGetTime();
		app->delta_time = current_time - app->last_frame_time;
		app->last_frame_time = current_time;
		fps_update(&app->fps_counter, app->delta_time, current_time);
		adaptive_sampler_should_sample(&app->fps_sampler,
		                               (float)app->delta_time,
		                               current_time, app->frame_count);
		action_notifier_update(&app->notifier, (float)app->delta_time);

		if (adaptive_sampler_is_finished(&app->fps_sampler,
		                                 current_time)) {
			adaptive_sampler_reset(&app->fps_sampler, current_time);
		}

		postprocess_update_time(&app->postprocess,
		                        (float)app->delta_time);

		app->camera.physics_accumulator += (float)app->delta_time;
		while (app->camera.physics_accumulator >=
		       app->camera.fixed_timestep) {
			camera_fixed_update(&app->camera);
			app->camera.physics_accumulator -=
			    app->camera.fixed_timestep;
		}

		float alpha = app->camera.rotation_smoothing;
		app->camera.yaw +=
		    (app->camera.yaw_target - app->camera.yaw) * alpha;
		app->camera.pitch +=
		    (app->camera.pitch_target - app->camera.pitch) * alpha;
		camera_update_vectors(&app->camera);

		if (app->subdivisions != last_subdiv) {
			icosphere_generate(&app->geometry, app->subdivisions);
			app_update_gpu_buffers(app);
#ifdef USE_SSBO_RENDERING
			ssbo_group_bind_mesh(&app->ssbo_group, app->sphere_vbo,
			                     app->sphere_nbo, app->sphere_ebo);
#else
			instanced_group_bind_mesh(
			    &app->instanced_group, app->sphere_vbo,
			    app->sphere_nbo, app->sphere_ebo);
#endif
			last_subdiv = app->subdivisions;
		}

		app_update(app);
		app_render(app);

		glfwSwapBuffers(app->window);
		glfwPollEvents();
	}
}

void app_update(App* app)
{
	AsyncRequest req;
	if (async_loader_poll(&req)) {
		app_finalize_environment_load(app, &req);
		if (req.data) {
			free(req.data); /* Data was allocated with malloc in
			                   texture_load_pixels */
		}
	}
	app_process_ibl_state_machine(app);
}

void app_render(App* app)
{
	// 1. Signaler le début de la frame pour traiter les résultats
	// précédents
	gpu_profiler_begin_frame(&app->gpu_profiler, app->frame_count);

	// 2. Démarrer la mesure globale de la frame
	gpu_profiler_start_stage(&app->gpu_profiler, "Total Frame",
	                         GPU_PROFILER_TOTAL_FRAME_COLOR);

	postprocess_begin(&app->postprocess);
	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);

	mat4 view;
	mat4 proj;
	mat4 view_proj;
	mat4 inv_view_proj;
	vec3 camera_pos = {app->camera.position[0], app->camera.position[1],
	                   app->camera.position[2]};
	camera_get_view_matrix(&app->camera, view);
	if (app->height > 0) {
		glm_perspective(glm_rad(app->camera.zoom),
		                (float)app->width / (float)app->height,
		                NEAR_PLANE, FAR_PLANE, proj);
	} else {
		glm_mat4_identity(proj);
	}
	glm_mat4_mul(proj, view, view_proj);
	glm_mat4_inv(view_proj, inv_view_proj);

#ifdef USE_TRANSPARENT_BILLBOARDS
	if (app->show_envmap) {
		gpu_profiler_start_stage(&app->gpu_profiler, "EnvMap",
		                         GPU_PROFILER_TOTAL_FRAME_COLOR);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_DEPTH_TEST);
		skybox_render(&app->skybox, app->skybox_shader,
		              app->hdr_texture, app->dummy_black_tex,
		              inv_view_proj, app->env_lod);
		glEnable(GL_DEPTH_TEST);

		gpu_profiler_end_stage(&app->gpu_profiler);
	}

	gpu_profiler_start_stage(&app->gpu_profiler, "Spheres",
	                         GPU_PROFILER_TOTAL_FRAME_COLOR);
	if (app->billboard_mode) {
		sphere_sorter_sort(&app->sphere_sorter, app->sphere_instances,
		                   app->sphere_instance_count,
		                   app->camera.position);
		billboard_group_update(&app->billboard_group,
		                       app->sphere_instances,
		                       app->sphere_instance_count);
	}

	if (app->billboard_mode) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// 1. Activer le Blending UNIQUEMENT pour la couleur (Attachment
		// 0) Cela permet à ton 'edgeFactor' de lisser les bords de la
		// sphère
		glEnablei(GL_BLEND, 0);

		// 2. Configurer l'équation de blend (toujours globale ou par
		// index si besoin) Pour l'alpha blending classique (lissage des
		// bords)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// 3. DÉSACTIVER explicitement le Blending pour la vélocité
		// (Attachment 1) C'est la clé : le buffer de vélocité recevra
		// les valeurs brutes du shader sans être multipliées par
		// l'alpha ou mixées avec le noir du fond.
		glDisablei(GL_BLEND, 1);

		app_render_billboards(app, view, proj, camera_pos);

		// Nettoyage après rendu (Optionnel mais propre)
		glDisablei(GL_BLEND, 0);
	} else {
		glPolygonMode(GL_FRONT_AND_BACK,
		              app->wireframe ? GL_LINE : GL_FILL);

		app_render_instanced(app, view, proj, camera_pos);
	}

	glDisable(GL_STENCIL_TEST);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#else
	gpu_profiler_start_stage(&app->gpu_profiler, "Spheres",
	                         GPU_PROFILER_TOTAL_FRAME_COLOR);
	if (app->billboard_mode) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		app_render_billboards(app, view, proj, camera_pos);
	} else {
		glPolygonMode(GL_FRONT_AND_BACK,
		              app->wireframe ? GL_LINE : GL_FILL);
		app_render_instanced(app, view, proj, camera_pos);
	}

	glDisable(GL_STENCIL_TEST);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if (app->show_envmap) {
		skybox_render(&app->skybox, app->skybox_shader,
		              app->hdr_texture, app->dummy_black_tex,
		              inv_view_proj, app->env_lod);
	}
#endif
	gpu_profiler_end_stage(&app->gpu_profiler);

	postprocess_end(&app->postprocess);

	postprocess_update_matrices(&app->postprocess, view_proj);

	app_render_ui(app);

	// 3. Arrêter la mesure de la frame
	gpu_profiler_end_stage(&app->gpu_profiler);

	// 4. Logique d'affichage toutes les 2 secondes
	double current_time = glfwGetTime();
	app_metrics_log_gpu_stats(&app->gpu_profiler, current_time);
}
