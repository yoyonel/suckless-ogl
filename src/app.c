#include "app.h"

#include "action_notifier.h"
#include "adaptive_sampler.h"
#include "app_env.h"
#include "app_input.h"
#include "app_scene.h"
#include "app_settings.h"
#include "app_ui.h"
#include "billboard_rendering.h"
#include "fps.h"
#include "gl_common.h"
#include "glad/glad.h"
#include "ibl_coordinator.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "render_utils.h"
#include "sphere_sorting.h"
#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/util.h>
#include <stb_image.h>
#include <stdlib.h>
#include <string.h>

static const char* const DEFAULT_ENV_FILENAME = "env.hdr";
#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif
#include "async_loader.h"
#include "camera.h"
#include "log.h"
#include "material.h"
#include "pbr.h"
#include "perf_mode.h"
#include "postprocess.h"
#include "shader.h"
#include "skybox.h"
#include "texture.h"
#include "tracy_gpu.h"
#include "ui.h"
#include "window.h"
#ifdef TRACY_ENABLE
#include "../deps/tracy/public/tracy/TracyC.h"
#endif
#include <GLFW/glfw3.h>

int app_init(App* app, int width, int height, const char* title)
{
	app->width = width;
	app->height = height;
	app->subdivisions = INITIAL_SUBDIVISIONS;
	app->wireframe = false;

	app->camera_enabled = true;
	app->env_lod = DEFAULT_ENV_LOD;
	app->show_info_overlay = true;
	app->show_exposure_debug = false;
	app->text_overlay_mode = 0;
	app->pbr_debug_mode = 0;
	app->is_fullscreen = false;
	app->show_help = false;
	app->show_envmap = true;
	app->billboard_mode = true;
	app->sorting_mode = SORTING_MODE_GPU_BITONIC;
	app->is_first_load = true;

	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	memset(&app->sphere_sorter, 0, sizeof(SphereSorter));

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
	/* Initialize Tracy Manager */
	tracy_manager_init(&app->tracy_mgr, width, height);

	/* Transition Snapshot Initialization (GL Context Ready) */
	/* Transition Initialization (Starts Black, fades in when IBL is done)
	 */
	app->transition_state = TRANSITION_WAIT_IBL;
	app->transition_alpha = 1.0F;
	app->transition_duration = DEFAULT_ENV_TRANSITION_DURATION;
	app->env_transition_mode = DEFAULT_ENV_TRANSITION_MODE;
	app->transition_snapshot_tex = 0;
	app->is_first_load = 1;

	app->current_exposure = 1.0F;
	app->dummy_black_tex =
	    render_utils_create_color_texture(0.0F, 0.0F, 0.0F, 0.0F);
	app->dummy_white_tex =
	    render_utils_create_color_texture(1.0F, 1.0F, 1.0F, 1.0F);

	glGenBuffers(2, app->upload_pbo);
	app->upload_pbo_idx = 0;
	app->upload_pbo_size[0] = 0;
	app->upload_pbo_size[1] = 0;

	app->lum_histogram_buffer =
	    malloc((size_t)(LUM_HISTOGRAM_MAP_SIZE * LUM_HISTOGRAM_MAP_SIZE) *
	           sizeof(float));
	if (!app->lum_histogram_buffer) {
		return 0;
	}

	app->brdf_lut_tex = build_brdf_lut_map(BRDF_LUT_MAP_SIZE);
	app->async_loader = async_loader_create(&app->tracy_mgr);
	if (!app->async_loader) {
		return 0;
	}

	app_scan_hdr_files(app);
	if (app->hdr_count > 0) {
		int default_idx = 0;
		for (int i = 0; i < app->hdr_count; i++) {
			if (strcmp(app->hdr_files[i], DEFAULT_ENV_FILENAME) ==
			    0) {
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

	app->skybox_shader =
	    shader_load("shaders/background.vert", "shaders/background.frag");
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

	app->billboard_uniforms.irradiance_map = shader_get_uniform_location(
	    app->pbr_billboard_shader, "irradianceMap");
	app->billboard_uniforms.prefilter_map = shader_get_uniform_location(
	    app->pbr_billboard_shader, "prefilterMap");
	app->billboard_uniforms.brdf_lut =
	    shader_get_uniform_location(app->pbr_billboard_shader, "brdfLUT");
	app->billboard_uniforms.debug_mode =
	    shader_get_uniform_location(app->pbr_billboard_shader, "debugMode");
	app->billboard_uniforms.cam_pos =
	    shader_get_uniform_location(app->pbr_billboard_shader, "camPos");
	app->billboard_uniforms.projection = shader_get_uniform_location(
	    app->pbr_billboard_shader, "projection");
	app->billboard_uniforms.view =
	    shader_get_uniform_location(app->pbr_billboard_shader, "view");
	app->billboard_uniforms.previous_view_proj =
	    shader_get_uniform_location(app->pbr_billboard_shader,
	                                "previousViewProj");
	app->billboard_uniforms.u_screen_size = shader_get_uniform_location(
	    app->pbr_billboard_shader, "u_screenSize");

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

	ibl_coordinator_init(&app->ibl_coord, app->shader_spmap,
	                     app->shader_irmap, app->shader_lum_pass1,
	                     app->shader_lum_pass2);

#ifdef USE_SSBO_RENDERING
	app_init_ssbo(app);
	app->pbr_ssbo_shader = shader_load("shaders/pbr_ibl_ssbo.vert",
	                                   "shaders/pbr_ibl_instanced.frag");
	if (!app->pbr_ssbo_shader) {
		return 0;
	}
	Shader* inst_shader = app->pbr_ssbo_shader;
#else
	app_init_instancing(app);
	app->pbr_instanced_shader = shader_load(
	    "shaders/pbr_ibl_instanced.vert", "shaders/pbr_ibl_instanced.frag");
	if (!app->pbr_instanced_shader) {
		return 0;
	}
	app_update_instancing_mode(app);
	Shader* inst_shader = app->pbr_instanced_shader;
#endif

	app->instanced_uniforms.irradiance_map =
	    shader_get_uniform_location(inst_shader, "irradianceMap");
	app->instanced_uniforms.prefilter_map =
	    shader_get_uniform_location(inst_shader, "prefilterMap");
	app->instanced_uniforms.brdf_lut =
	    shader_get_uniform_location(inst_shader, "brdfLUT");
	app->instanced_uniforms.debug_mode =
	    shader_get_uniform_location(inst_shader, "debugMode");
	app->instanced_uniforms.cam_pos =
	    shader_get_uniform_location(inst_shader, "camPos");
	app->instanced_uniforms.projection =
	    shader_get_uniform_location(inst_shader, "projection");
	app->instanced_uniforms.view =
	    shader_get_uniform_location(inst_shader, "view");
	app->instanced_uniforms.previous_view_proj =
	    shader_get_uniform_location(inst_shader, "previousViewProj");

	app->debug_uniforms.projection =
	    shader_get_uniform_location(app->debug_line_shader, "projection");
	app->debug_uniforms.view =
	    shader_get_uniform_location(app->debug_line_shader, "view");
	app->debug_uniforms.u_stippled =
	    shader_get_uniform_location(app->debug_line_shader, "u_stippled");
	app->debug_uniforms.u_billboard_mode = shader_get_uniform_location(
	    app->debug_line_shader, "u_billboardMode");
	app->debug_uniforms.u_use_instance_col = shader_get_uniform_location(
	    app->debug_line_shader, "u_useInstanceColor");
	app->debug_uniforms.u_color =
	    shader_get_uniform_location(app->debug_line_shader, "u_color");

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
	gpu_profiler_ui_init(&app->timeline_ui);
	effect_benchmark_init(&app->effect_bench, &app->postprocess,
	                      &app->gpu_profiler);
	app->log_gpu_metrics = 0; /* Console logging off by default */

	return 1;
}

static void app_cleanup_rendering_groups(App* app)
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
}

static void app_cleanup_pbr_shaders(App* app)
{
	SHADER_SAFE_DESTROY(app->pbr_instanced_shader);
	SHADER_SAFE_DESTROY(app->pbr_billboard_shader);
#ifdef USE_SSBO_RENDERING
	SHADER_SAFE_DESTROY(app->pbr_ssbo_shader);
#endif
}

static void app_cleanup_util_shaders(App* app)
{
	SHADER_SAFE_DESTROY(app->debug_shader);
	SHADER_SAFE_DESTROY(app->debug_line_shader);
	SHADER_SAFE_DESTROY(app->skybox_shader);
	GL_SAFE_DELETE_PROGRAM(app->shader_spmap);
	GL_SAFE_DELETE_PROGRAM(app->shader_irmap);
	GL_SAFE_DELETE_PROGRAM(app->shader_lum_pass1);
	GL_SAFE_DELETE_PROGRAM(app->shader_lum_pass2);
}

static void app_cleanup_gpu_vaos(App* app)
{
	GL_SAFE_DELETE_VAO(app->sphere_vao);
	GL_SAFE_DELETE_VAO(app->empty_vao);
}

static void app_cleanup_gpu_vbos(App* app)
{
	GL_SAFE_DELETE_BUFFER(app->sphere_vbo);
	GL_SAFE_DELETE_BUFFER(app->sphere_nbo);
	GL_SAFE_DELETE_BUFFER(app->sphere_ebo);
	GL_SAFE_DELETE_BUFFER(app->wire_cube_vbo);
	GL_SAFE_DELETE_BUFFER(app->wire_quad_vbo);
	GL_SAFE_DELETE_BUFFER(app->quad_vbo);
	GL_SAFE_DELETE_BUFFERS(2, app->lum_ssbo);
}

static void app_cleanup_gpu_textures(App* app)
{
	GL_SAFE_DELETE_TEXTURE(app->hdr_texture);
	GL_SAFE_DELETE_TEXTURE(app->recycled_hdr_tex);
	GL_SAFE_DELETE_TEXTURE(app->brdf_lut_tex);
	GL_SAFE_DELETE_TEXTURE(app->spec_prefiltered_tex);
	GL_SAFE_DELETE_TEXTURE(app->irradiance_tex);
	GL_SAFE_DELETE_TEXTURE(app->dummy_black_tex);
	GL_SAFE_DELETE_TEXTURE(app->dummy_white_tex);
	GL_SAFE_DELETE_TEXTURE(app->transition_snapshot_tex);
}

static void app_cleanup_gpu_pbos(App* app)
{
	GL_SAFE_DELETE_BUFFER(app->exposure_pbo);
	GL_SAFE_DELETE_BUFFER(app->histogram_pbo);
	GL_SAFE_DELETE_BUFFERS(2, app->upload_pbo);
}

void app_cleanup(App* app)
{
	if (!app) {
		return;
	}

	/* 1. High-level systems first (may depend on textures/shaders) */
	ui_destroy(&app->ui);
	postprocess_cleanup(&app->postprocess);

	/* Async Loader Shutdown before other resources */
	async_loader_destroy(app->async_loader);
	app->async_loader = NULL;

	/* 2. Scene / Rendering groups */
	app_cleanup_rendering_groups(app);

	if (app->material_lib) {
		material_free_lib(app->material_lib);
		app->material_lib = NULL;
	}

	/* 3. Common low-level resources */
	app_cleanup_pbr_shaders(app);
	app_cleanup_util_shaders(app);
	app_cleanup_gpu_vaos(app);
	app_cleanup_gpu_vbos(app);
	app_cleanup_gpu_textures(app);
	app_cleanup_gpu_pbos(app);

	ibl_coordinator_cleanup(&app->ibl_coord);

	adaptive_sampler_cleanup(&app->fps_sampler);

	if (app->hdr_files) {
		for (int i = 0; i < app->hdr_count; i++) {
			free(app->hdr_files[i]);
			app->hdr_files[i] = NULL;
		}
		free(app->hdr_files);
		app->hdr_files = NULL;
		app->hdr_count = 0;
	}
	if (app->lum_histogram_buffer) {
		free(app->lum_histogram_buffer);
		app->lum_histogram_buffer = NULL;
	}

	perf_mode_cleanup(&app->perf_context);

	gpu_profiler_cleanup(&app->gpu_profiler);
	gpu_profiler_ui_cleanup(&app->timeline_ui);

	window_destroy(app->window);
	app->window = NULL;

	tracy_manager_cleanup(&app->tracy_mgr);
}

void app_run(App* app)
{
	int last_subdiv = -1;
	while (!glfwWindowShouldClose(app->window)) {
		{
#ifdef TRACY_ENABLE
			TracyCZoneN(poll_ctx, "GLFW PollEvents (Start)", 1);
#endif
			glfwPollEvents();
#ifdef TRACY_ENABLE
			TracyCZoneEnd(poll_ctx);
#endif
		}

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

		{
#ifdef TRACY_ENABLE
			TracyCZoneN(update_ctx, "App Update", 1);
#endif
			app_update(app);
#ifdef TRACY_ENABLE
			TracyCZoneEnd(update_ctx);
#endif
		}

		{
#ifdef TRACY_ENABLE
			TracyCZoneN(render_ctx, "App Render", 1);
#endif
			app_render(app);
#ifdef TRACY_ENABLE
			TracyCZoneEnd(render_ctx);
#endif
		}

		{
#ifdef TRACY_ENABLE
			TracyCZoneN(tracy_mark_ctx, "Tracy Mark/Screenshots",
			            1);
#endif
			tracy_manager_update_screenshots(&app->tracy_mgr, app);
#ifdef TRACY_ENABLE
			TracyCZoneEnd(tracy_mark_ctx);
#endif
		}

		{
#ifdef TRACY_ENABLE
			TracyCZoneN(swap_ctx, "GLFW SwapBuffers", 1);
#endif
			glfwSwapBuffers(app->window);
#ifdef TRACY_ENABLE
			TracyCZoneEnd(swap_ctx);
#endif
		}

		{
#ifdef TRACY_ENABLE
			TracyCZoneN(gpu_collect_ctx, "Tracy GPU Collect", 1);
#endif
			tracy_gpu_collect();
#ifdef TRACY_ENABLE
			TracyCZoneEnd(gpu_collect_ctx);
#endif
		}
	}
}

void app_update(App* app)
{
	/* Deferred texture pre-allocation: runs in the frame AFTER
	 * PBO setup, so glTexImage2D doesn't pile up on the same
	 * frame as PBO Setup & Map.
	 */
	if (app->pending_prealloc_w > 0) {
		app->recycled_hdr_tex = texture_preallocate_hdr(
		    app->pending_prealloc_w, app->pending_prealloc_h,
		    app->recycled_hdr_tex);
		app->pending_prealloc_w = 0;
		app->pending_prealloc_h = 0;
	}

	AsyncRequest req;
	if (async_loader_poll(app->async_loader, &req)) {
		if (req.state == ASYNC_WAITING_FOR_PBO) {
			/* Step 1: Main thread provides mapped PBO */
			/* Use ping-pong index to avoid stalling on previous
			 * frame's upload */
			int pbo_idx = app->upload_pbo_idx;
			size_t size = (size_t)req.width * (size_t)req.height *
			              4 * sizeof(uint16_t);
#ifdef TRACY_ENABLE
			TracyCZoneN(pbo_ctx, "PBO Setup & Map", 1);
#endif
			texture_ensure_pbo(&app->upload_pbo[pbo_idx],
			                   &app->upload_pbo_size[pbo_idx],
			                   (GLsizeiptr)size);
			void* ptr =
			    texture_map_pbo(app->upload_pbo[pbo_idx], size);
#ifdef TRACY_ENABLE
			TracyCZoneEnd(pbo_ctx);
#endif
			if (ptr) {
				async_loader_provide_pbo(
				    app->async_loader, ptr,
				    app->upload_pbo[pbo_idx]);
				/* Advance index for next request */
				app->upload_pbo_idx =
				    (app->upload_pbo_idx + 1) % 2;

				/* Schedule deferred pre-allocation for
				 * NEXT frame. This spreads glTexStorage2D
				 * cost away from PBO setup.
				 */
				app->pending_prealloc_w = req.width;
				app->pending_prealloc_h = req.height;
			} else {
				LOG_ERROR("suckless-ogl.app",
				          "Failed to map PBO for async upload");
				async_loader_cancel(app->async_loader);
			}
		} else if (req.state == ASYNC_READY) {
			/* Step 2: Begin multi-frame finalize process */
			app->current_env_req = req;
			app->env_map_loading_step = 1;
		}
	}

	if (app->env_map_loading_step > 0) {
		app_process_env_map_loading_step(app);
	}

	app_process_ibl_state_machine(app);
	app_update_transition(app);
}

static inline void stencil_begin_object_pass(void)
{
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilFunc(GL_ALWAYS, 1, DEFAULT_STENCIL_MASK);
	glStencilMask(DEFAULT_STENCIL_MASK);
}

void app_render(App* app)
{
	// 1. Signaler le début de la frame pour traiter les résultats
	// précédents
	// 1. Signaler le début de la frame pour traiter les résultats
	// précédents
	bool profiling_enabled =
	    app->timeline_ui.visible || app->log_gpu_metrics ||
	    effect_benchmark_is_running(&app->effect_bench);
	gpu_profiler_set_enabled(&app->gpu_profiler, profiling_enabled);
	gpu_profiler_begin_frame(&app->gpu_profiler, app->frame_count);

#ifdef TRACY_ENABLE
	TracyCFrameMark;
#endif

	/* Effect benchmark: read previous frame's profiler results */
	if (effect_benchmark_update(&app->effect_bench)) {
		action_notifier_push(&app->notifier,
		                     "FX Benchmark: Done (see log)",
		                     NOTIF_DUR_LONG);
	}

	// 2. Démarrer la mesure globale de la frame
	GPU_STAGE_PROFILER(&app->gpu_profiler, "Total Frame",
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
		GPU_STAGE_PROFILER(&app->gpu_profiler, "EnvMap",
		                   GPU_PROFILER_ENV_COLOR);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_DEPTH_TEST);
		skybox_render(&app->skybox, app->skybox_shader,
		              app->hdr_texture, app->dummy_black_tex,
		              inv_view_proj, app->env_lod);
		glEnable(GL_DEPTH_TEST);
	}

	{
		GPU_STAGE_PROFILER(&app->gpu_profiler, "Spheres",
		                   GPU_PROFILER_SCENE_COLOR);

		stencil_begin_object_pass();

		if (app->billboard_mode) {
			GLuint sorted_ssbo = 0;
			switch (app->sorting_mode) {
				case SORTING_MODE_CPU_QSORT:
					sorted_ssbo = sphere_sorter_sort_cpu(
					    &app->sphere_sorter,
					    app->sphere_instances,
					    app->sphere_instance_count,
					    app->camera.position);
					break;
				case SORTING_MODE_CPU_RADIX:
					sorted_ssbo =
					    sphere_sorter_sort_cpu_radix(
					        &app->sphere_sorter,
					        app->sphere_instances,
					        app->sphere_instance_count,
					        app->camera.position);
					break;
				case SORTING_MODE_GPU_BITONIC:
				default:
					sorted_ssbo = sphere_sorter_sort_gpu(
					    &app->sphere_sorter,
					    app->sphere_instances,
					    app->sphere_instance_count,
					    app->camera.position);
					break;
			}
			billboard_group_update_from_buffer(
			    &app->billboard_group, sorted_ssbo,
			    app->sphere_instance_count);

			// 1. Activer le Blending UNIQUEMENT pour la couleur
			// (Attachment 0) Cela permet à ton 'edgeFactor' de
			// lisser les bords de la sphère
			glEnablei(GL_BLEND, 0);

			// 2. Configurer l'équation de blend (toujours globale
			// ou par index si besoin) Pour l'alpha blending
			// classique (lissage des bords)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// 3. DÉSACTIVER explicitement le Blending pour la
			// vélocité (Attachment 1) C'est la clé : le buffer de
			// vélocité recevra les valeurs brutes du shader sans
			// être multipliées par l'alpha ou mixées avec le noir
			// du fond.
			glDisablei(GL_BLEND, 1);

			app_render_billboards(app, view, proj, camera_pos);

			// Nettoyage après rendu (Optionnel mais propre)
			glDisablei(GL_BLEND, 0);
		} else {
			glPolygonMode(GL_FRONT_AND_BACK,
			              app->wireframe ? GL_LINE : GL_FILL);

			app_render_instanced(app, view, proj, camera_pos);

			if (app->wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
		}

		glDisable(GL_STENCIL_TEST);
	}
#else
	{
		GPU_STAGE_PROFILER(&app->gpu_profiler, "Spheres",
		                   GPU_PROFILER_TOTAL_FRAME_COLOR);

		stencil_begin_object_pass();

		if (app->billboard_mode) {
			app_render_billboards(app, view, proj, camera_pos);
		} else {
			glPolygonMode(GL_FRONT_AND_BACK,
			              app->wireframe ? GL_LINE : GL_FILL);
			app_render_instanced(app, view, proj, camera_pos);

			if (app->wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
		}

		glDisable(GL_STENCIL_TEST);

		if (app->show_envmap) {
			skybox_render(&app->skybox, app->skybox_shader,
			              app->hdr_texture, app->dummy_black_tex,
			              inv_view_proj, app->env_lod);
		}
	}
#endif

	postprocess_end(&app->postprocess);

	postprocess_update_matrices(&app->postprocess, view_proj);

	{
		GPU_STAGE_PROFILER(&app->gpu_profiler, "UI Overlay",
		                   GPU_PROFILER_UI_COLOR);

		/* Render Transition Overlay */
		if (app->transition_state != TRANSITION_IDLE) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_DEPTH_TEST);

			shader_use(app->debug_shader);
			shader_set_int(app->debug_shader, "u_tex", 0);
			shader_set_float(app->debug_shader, "u_alpha",
			                 app->transition_alpha);
			shader_set_int(app->debug_shader, "u_bypass_processing",
			               1);
			shader_set_float(app->debug_shader, "lod", 0.0F);

			glActiveTexture(GL_TEXTURE0);
			if (app->env_transition_mode ==
			        ENV_TRANSITION_CROSSFADE &&
			    app->transition_snapshot_tex != 0 &&
			    app->transition_state == TRANSITION_FADE_IN) {
				/* Crossfade: Bind snapshot texture */
				glBindTexture(GL_TEXTURE_2D,
				              app->transition_snapshot_tex);
			} else {
				/* Black Screen / Initial Load: Bind dummy black
				 */
				glBindTexture(GL_TEXTURE_2D,
				              app->dummy_black_tex);
			}

			glBindVertexArray(app->quad_vbo);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			glBindVertexArray(0);

			glEnable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);
		}

		app_render_ui(app);
	}

	// 4. Logique d'affichage et animations
	double current_time = glfwGetTime();
	gpu_profiler_ui_update(&app->timeline_ui, &app->gpu_profiler,
	                       app->delta_time, current_time,
	                       (bool)app->log_gpu_metrics);
}
