#include "app.h"

#include "action_notifier.h"
#include "adaptive_sampler.h"
#include "app_env.h"
#include "app_input.h"
#include "app_settings.h"
#include "app_ui.h"
#include "async_loader.h"
#include "camera.h"
#include "fps.h"
#include "gl_common.h"
#include "glad/glad.h"
#include "log.h"
#include "perf_mode.h"
#include "postprocess.h"
#include "scene.h"
#include "texture.h"
#include "tracy_gpu.h"
#include "ui.h"
#include "window.h"
#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/util.h>
#include <stb_image.h>
#include <stdlib.h>
#include <string.h>
#ifdef TRACY_ENABLE
#include "../deps/tracy/public/tracy/TracyC.h"
#endif
#include <GLFW/glfw3.h>

static const char* const DEFAULT_ENV_FILENAME = "env.hdr";

int app_init(App* app, int width, int height, const char* title)
{
	app->width = width;
	app->height = height;

	app->camera_enabled = true;
	app->show_info_overlay = true;
	app->show_exposure_debug = false;
	app->text_overlay_mode = 0;
	app->is_fullscreen = false;
	app->show_help = false;
	app->env_mgr.is_first_load = true;

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
	app->env_mgr.transition_state = TRANSITION_WAIT_IBL;
	app->env_mgr.transition_alpha = 1.0F;
	app->env_mgr.transition_duration = DEFAULT_ENV_TRANSITION_DURATION;
	app->env_mgr.env_transition_mode = DEFAULT_ENV_TRANSITION_MODE;
	app->env_mgr.is_first_load = 1;

	app->current_exposure = 1.0F;

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

	app->async_loader = async_loader_create(&app->tracy_mgr);
	if (!app->async_loader) {
		return 0;
	}

	if (!scene_init(&app->scene)) {
		return 0;
	}

	/* Initial HDR load */
	if (app->scene.hdr_count > 0) {
		int default_idx = 0;
		for (int i = 0; i < app->scene.hdr_count; i++) {
			if (strcmp(app->scene.hdr_files[i],
			           DEFAULT_ENV_FILENAME) == 0) {
				default_idx = i;
				break;
			}
		}
		app->scene.current_hdr_index = default_idx;
		app_load_env_map(app, app->scene.hdr_files[default_idx]);
	}

	app->u_metallic = DEFAULT_METALLIC;
	app->u_roughness = DEFAULT_ROUGHNESS;
	app->u_ao = DEFAULT_AO;
	app->u_exposure = DEFAULT_EXPOSURE;
	glEnable(GL_DEPTH_TEST);

	fps_init(&app->fps_counter, DEFAULT_FPS_SMOOTHING, DEFAULT_FPS_WINDOW);
	adaptive_sampler_init(&app->fps_sampler, DEFAULT_FPS_WINDOW,
	                      DEFAULT_FPS_SAMPLER_SIZE, DEFAULT_FPS_TARGET);
	app->last_frame_time = glfwGetTime();

	ui_init(&app->ui, "assets/fonts/FiraCode-Regular.ttf",
	        DEFAULT_FONT_SIZE);

	if (!postprocess_init(&app->postprocess, &app->gpu_profiler, width,
	                      height)) {
		return 0;
	}
	postprocess_set_dummy_textures(&app->postprocess,
	                               app->scene.dummy_black_tex);
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
	scene_cleanup(&app->scene);

	/* 3. Common low-level resources */
	GL_SAFE_DELETE_BUFFER(app->exposure_pbo);
	GL_SAFE_DELETE_BUFFER(app->histogram_pbo);
	GL_SAFE_DELETE_BUFFERS(2, app->upload_pbo);

	adaptive_sampler_cleanup(&app->fps_sampler);

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

		if (app->scene.subdivisions != last_subdiv) {
			icosphere_generate(&app->scene.geometry,
			                   app->scene.subdivisions);
			scene_update_gpu_buffers(&app->scene);

#ifdef USE_SSBO_RENDERING
			ssbo_group_bind_mesh(
			    &app->scene.ssbo_group, app->scene.sphere_vbo,
			    app->scene.sphere_nbo, app->scene.sphere_ebo);
#else
			instanced_group_bind_mesh(
			    &app->scene.instanced_group, app->scene.sphere_vbo,
			    app->scene.sphere_nbo, app->scene.sphere_ebo);
#endif
			last_subdiv = app->scene.subdivisions;
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
		app->scene.recycled_hdr_tex = texture_preallocate_hdr(
		    app->pending_prealloc_w, app->pending_prealloc_h,
		    app->scene.recycled_hdr_tex);
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
			app->env_mgr.current_env_req = req;
			app->env_mgr.env_map_loading_step = 1;
		}
	}

	if (app->env_mgr.env_map_loading_step > 0) {
		app_process_env_map_loading_step(app);
	}

	app_process_ibl_state_machine(app);
	app_update_transition(app);
}

void app_render(App* app)
{
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

	scene_render(&app->scene, view, proj, camera_pos,
	             app->postprocess.motion_blur_fx.previous_view_proj,
	             app->width, app->height);

	postprocess_end(&app->postprocess);

	postprocess_update_matrices(&app->postprocess, view_proj);

	{
		GPU_STAGE_PROFILER(&app->gpu_profiler, "UI Overlay",
		                   GPU_PROFILER_UI_COLOR);

		/* Render Transition Overlay */
		if (app->env_mgr.transition_state != TRANSITION_IDLE) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_DEPTH_TEST);

			shader_use(app->scene.debug_shader);
			shader_set_int(app->scene.debug_shader, "u_tex", 0);
			shader_set_float(app->scene.debug_shader, "u_alpha",
			                 app->env_mgr.transition_alpha);
			shader_set_int(app->scene.debug_shader,
			               "u_bypass_processing", 1);
			shader_set_float(app->scene.debug_shader, "lod", 0.0F);

			glActiveTexture(GL_TEXTURE0);
			if (app->env_mgr.env_transition_mode ==
			        ENV_TRANSITION_CROSSFADE &&
			    app->scene.transition_snapshot_tex != 0 &&
			    app->env_mgr.transition_state ==
			        TRANSITION_FADE_IN) {
				/* Crossfade: Bind snapshot texture */
				glBindTexture(
				    GL_TEXTURE_2D,
				    app->scene.transition_snapshot_tex);
			} else {
				/* Black Screen / Initial Load: Bind dummy black
				 */
				glBindTexture(GL_TEXTURE_2D,
				              app->scene.dummy_black_tex);
			}

			glBindVertexArray(app->scene.quad_vbo);
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
