#include "app.h"

#include "action_notifier.h"
#include "app_input.h"
#include "app_input_state.h"
#include "app_profiling.h"
#include "app_settings.h"
#include "app_ui.h"
#include "async_loader.h"
#include "gl_common.h"
#include "glad/glad.h"
#include "postprocess.h"
#include "profiler.h"
#include "renderer.h"
#include "scene.h"
#include "texture.h"
#include "tracy_gpu.h"
#include "window.h"
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stdlib.h>
#include <string.h>

static const char* const DEFAULT_ENV_FILENAME = "env.hdr";

int app_init(App* app, int width, int height, const char* title)
{
	app->width = width;
	app->height = height;

	app->input = calloc(1, sizeof(*app->input));
	if (!app->input) {
		return 0;
	}
	app->profiling = calloc(1, sizeof(*app->profiling));
	if (!app->profiling) {
		free(app->input);
		app->input = NULL;
		return 0;
	}

	app_input_state_init(app->input);
	app->is_fullscreen = false;
	app->resize_pending = 0;
	app->scene.config.specular_aa_enabled = DEFAULT_SPECULAR_AA_ENABLED;
	app->env_mgr.is_first_load = true;

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

	if (app->input->camera_enabled) {
		glfwSetInputMode(app->window, GLFW_CURSOR,
		                 GLFW_CURSOR_DISABLED);
	}

	/* Initialize all profiling sub-systems (needs GL context) */
	app_profiling_init(app->profiling, width, height);

	/* Transition Snapshot Initialization (GL Context Ready) */
	/* Transition Initialization (Starts Black, fades in when IBL is done)
	 */
	app->env_mgr.transition_state = TRANSITION_WAIT_IBL;
	app->env_mgr.transition_alpha = 1.0F;
	app->env_mgr.transition_duration = DEFAULT_ENV_TRANSITION_DURATION;
	app->env_mgr.env_transition_mode = DEFAULT_ENV_TRANSITION_MODE;
	app->env_mgr.is_first_load = 1;

	async_coordinator_init(&app->async_coord);

	app->lum_histogram_buffer =
	    malloc((size_t)(LUM_HISTOGRAM_MAP_SIZE * LUM_HISTOGRAM_MAP_SIZE) *
	           sizeof(float));
	if (!app->lum_histogram_buffer) {
		return 0;
	}

	app->async_loader = async_loader_create(&app->profiling->tracy_mgr);
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
		env_manager_load(&app->env_mgr, app->async_loader,
		                 app->scene.hdr_files[default_idx]);
	}

	app->u_metallic = DEFAULT_METALLIC;
	app->u_roughness = DEFAULT_ROUGHNESS;
	app->u_ao = DEFAULT_AO;
	app->u_exposure = DEFAULT_EXPOSURE;
	glEnable(GL_DEPTH_TEST);

	app->last_frame_time = glfwGetTime();
	app_ui_init(&app->overlay);

	if (!postprocess_init(&app->postprocess, &app->profiling->gpu_profiler,
	                      width, height)) {
		return 0;
	}
	postprocess_set_dummy_textures(&app->postprocess,
	                               app->scene.gpu.dummy_black_tex);
	postprocess_set_exposure(&app->postprocess,
	                         app->postprocess.readback.auto_threshold);
	postprocess_enable(&app->postprocess, POSTFX_FXAA);

#ifdef ENABLE_SHADER_OPTIMIZATION
	postprocess_compile_optimized(&app->postprocess,
	                              app->postprocess.active_effects);
#endif

	action_notifier_init(&app->notifier);

	effect_benchmark_init(&app->effect_bench, &app->postprocess,
	                      &app->profiling->gpu_profiler);

	return 1;
}

void app_cleanup(App* app)
{
	if (!app) {
		return;
	}

	/* 1. High-level systems first (may depend on textures/shaders) */
	app_ui_cleanup(&app->overlay);
	postprocess_cleanup(&app->postprocess);

	/* Async Loader Shutdown before other resources */
	async_loader_destroy(app->async_loader);
	app->async_loader = NULL;

	/* 2. Scene / Rendering groups */
	scene_cleanup(&app->scene);

	/* 3. Common low-level resources */

	async_coordinator_cleanup(&app->async_coord);

	app_input_state_cleanup(app->input);
	free(app->input);
	app->input = NULL;

	if (app->lum_histogram_buffer) {
		free(app->lum_histogram_buffer);
		app->lum_histogram_buffer = NULL;
	}

	app_profiling_cleanup(app->profiling);
	free(app->profiling);
	app->profiling = NULL;

	window_destroy(app->window);
	app->window = NULL;
}

static void app_render_ui_trampoline(void* user_data)
{
	app_render_ui((const App*)user_data);
}

void app_run(App* app)
{
	int last_subdiv = -1;
	while (!glfwWindowShouldClose(app->window)) {
		{
			PROFILE_ZONE(poll_ctx, "GLFW PollEvents (Start)");
			glfwPollEvents();
			PROFILE_ZONE_END(poll_ctx);
		}

		/* Process deferred resize from framebuffer_size_callback.
		 * Heavy GPU work (FBO/texture recreation) is done here, safely
		 * outside any GLFW callback context, after the mode switch and
		 * event processing are fully complete. */
		if (app->resize_pending) {
			PROFILE_ZONE(resize_ctx, "PostProcess Resize");
			postprocess_resize(&app->postprocess,
			                   app->pending_width,
			                   app->pending_height);
			app->resize_pending = 0;
			PROFILE_ZONE_END(resize_ctx);
		}

		bool profiling_enabled =
		    (app->profiling->timeline_ui.visible ||
		     app->profiling->log_gpu_metrics != 0 ||
		     effect_benchmark_is_running(&app->effect_bench)) != 0;
		gpu_profiler_set_enabled(&app->profiling->gpu_profiler,
		                         profiling_enabled);
		gpu_profiler_begin_frame(&app->profiling->gpu_profiler,
		                         app->frame_count);

		/* 1. Global Measure (includes CPU update and GPU swap) */
		GPU_STAGE_PROFILER(&app->profiling->gpu_profiler, "Total Frame",
		                   GPU_PROFILER_TOTAL_FRAME_COLOR);

		{
			PROFILE_ZONE(timing_ctx, "Frame Timing");
			app->frame_count++;
			double current_time = glfwGetTime();
			app->delta_time = current_time - app->last_frame_time;
			app->last_frame_time = current_time;
			fps_update(&app->profiling->fps_counter,
			           app->delta_time, current_time);
			adaptive_sampler_should_sample(
			    &app->input->fps_sampler, (float)app->delta_time,
			    current_time, app->frame_count);
			if (adaptive_sampler_is_finished(
			        &app->input->fps_sampler, current_time)) {
				adaptive_sampler_reset(&app->input->fps_sampler,
				                       current_time);
			}
			PROFILE_ZONE_END(timing_ctx);
		}

		{
			PROFILE_ZONE(ui_notif_ctx, "UI & Notifier Update");
			action_notifier_update(&app->notifier,
			                       (float)app->delta_time);
			app_ui_update(&app->overlay, app->delta_time);
			postprocess_update_time(&app->postprocess,
			                        (float)app->delta_time);
			gpu_usage_update(&app->profiling->gpu_usage);
			PROFILE_ZONE_END(ui_notif_ctx);
		}

		{
			PROFILE_ZONE(camera_ctx, "Camera Physics");
			GamepadActions gp_actions = {0, 0, 0};
			if (app->input->camera_enabled) {
				gamepad_input_poll(&app->input->gamepad,
				                   &gp_actions);
			}
			if (gp_actions.env_next || gp_actions.env_prev) {
				AppInputContext env_ctx = {
				    .window = app->window,
				    .scene = &app->scene,
				    .env_mgr = &app->env_mgr,
				    .notifier = &app->notifier,
				    .async_loader = app->async_loader,
				};
				if (gp_actions.env_next) {
					app_handle_env_input(&env_ctx,
					                     GLFW_PRESS, 0,
					                     GLFW_KEY_PAGE_UP);
				}
				if (gp_actions.env_prev) {
					app_handle_env_input(
					    &env_ctx, GLFW_PRESS, 0,
					    GLFW_KEY_PAGE_DOWN);
				}
			}
			if (gp_actions.camera_reset) {
				camera_init(&app->input->camera,
				            DEFAULT_CAMERA_DISTANCE,
				            DEFAULT_CAMERA_YAW,
				            DEFAULT_CAMERA_PITCH);
				app->scene.config.env_lod = DEFAULT_ENV_LOD;
				action_notifier_push(&app->notifier,
				                     "Camera & LOD Reset",
				                     NOTIF_DUR_LONG);
			}
			app->input->camera.physics_accumulator +=
			    (float)app->delta_time;
			while (app->input->camera.physics_accumulator >=
			       app->input->camera.fixed_timestep) {
				camera_build_keyboard_input(
				    &app->input->camera);
				gamepad_write_input(&app->input->gamepad,
				                    &app->input->camera);
				camera_fixed_update(&app->input->camera);
				app->input->camera.physics_accumulator -=
				    app->input->camera.fixed_timestep;
			}

			float alpha = app->input->camera.rotation_smoothing;
			app->input->camera.yaw +=
			    (app->input->camera.yaw_target -
			     app->input->camera.yaw) *
			    alpha;
			app->input->camera.pitch +=
			    (app->input->camera.pitch_target -
			     app->input->camera.pitch) *
			    alpha;
			camera_update_vectors(&app->input->camera);
			PROFILE_ZONE_END(camera_ctx);
		}

		if (app->scene.config.subdivisions != last_subdiv) {
			PROFILE_ZONE(ico_ctx, "Icosphere Regen");
			icosphere_generate(&app->scene.geometry,
			                   app->scene.config.subdivisions);
			scene_update_gpu_buffers(&app->scene);

#ifdef USE_SSBO_RENDERING
			ssbo_group_bind_mesh(&app->scene.ssbo_group,
			                     app->scene.gpu.icosphere_vbo,
			                     app->scene.gpu.icosphere_nbo,
			                     app->scene.gpu.icosphere_ebo);
#else
			instanced_group_bind_mesh(&app->scene.instanced_group,
			                          app->scene.gpu.icosphere_vbo,
			                          app->scene.gpu.icosphere_nbo,
			                          app->scene.gpu.icosphere_ebo);
#endif
			last_subdiv = app->scene.config.subdivisions;
			PROFILE_ZONE_END(ico_ctx);
		}

		{
			PROFILE_ZONE(nbody_ctx, "NBody Physics");
			scene_nbody_update(&app->scene, (float)app->delta_time);
			PROFILE_ZONE_END(nbody_ctx);
		}

		{
			PROFILE_ZONE(update_ctx, "App Update");
			app_update(app);
			PROFILE_ZONE_END(update_ctx);
		}

		{
			PROFILE_ZONE(render_ctx, "App Render");
			RenderContext rctx = {
			    .scene = &app->scene,
			    .postprocess = &app->postprocess,
			    .camera = &app->input->camera,
			    .profiler = &app->profiling->gpu_profiler,
			    .profiler_ui = &app->profiling->timeline_ui,
			    .env_mgr = &app->env_mgr,
			    .notifier = &app->notifier,
			    .effect_bench = &app->effect_bench,
			    .width = app->width,
			    .height = app->height,
			    .delta_time = app->delta_time,
			    .frame_count = app->frame_count,
			    .log_gpu_metrics = app->profiling->log_gpu_metrics,
			    .render_ui = app_render_ui_trampoline,
			    .render_ui_data = app,
			};
			renderer_draw_frame(&rctx);
			PROFILE_ZONE_END(render_ctx);
		}

		{
			PROFILE_ZONE(tracy_mark_ctx, "Tracy Mark/Screenshots");
			tracy_manager_update_screenshots(
			    &app->profiling->tracy_mgr, app);
			PROFILE_ZONE_END(tracy_mark_ctx);
		}

		{
			GPU_STAGE_PROFILER(&app->profiling->gpu_profiler,
			                   "Swap Buffers",
			                   GPU_PROFILER_UI_COLOR);
			PROFILE_ZONE(swap_ctx, "GLFW SwapBuffers");
			glfwSwapBuffers(app->window);
			PROFILE_ZONE_END(swap_ctx);
		}

		{
			PROFILE_ZONE(gpu_collect_ctx, "Tracy GPU Collect");
			tracy_gpu_collect();
			PROFILE_ZONE_END(gpu_collect_ctx);
		}
	}
}

void app_update(App* app)
{
	/* Deferred texture pre-allocation: runs in the frame AFTER
	 * PBO setup, so glTexImage2D doesn't pile up on the same
	 * frame as PBO Setup & Map.
	 */
	if (app->async_coord.pending_prealloc_w > 0) {
		app->scene.gpu.recycled_hdr_tex =
		    texture_preallocate_hdr(app->async_coord.pending_prealloc_w,
		                            app->async_coord.pending_prealloc_h,
		                            app->scene.gpu.recycled_hdr_tex);
		app->async_coord.pending_prealloc_w = 0;
		app->async_coord.pending_prealloc_h = 0;
	}

	AsyncRequest ready_req;
	if (async_coordinator_update(&app->async_coord, app->async_loader,
	                             &ready_req)) {
		/* Step 2: Begin multi-frame finalize process */
		app->env_mgr.current_env_req = ready_req;
		app->env_mgr.env_map_loading_step = 1;
	}

	if (app->env_mgr.env_map_loading_step > 0) {
		env_manager_process_loading_step(
		    &app->env_mgr, &app->scene.gpu.recycled_hdr_tex,
		    &app->scene.lighting.ibl_coord);
	}

	env_manager_update_ibl(&app->env_mgr, &app->scene, &app->postprocess,
	                       app->frame_count, app->width, app->height);
	env_manager_update_transition(&app->env_mgr, &app->scene,
	                              &app->postprocess, app->delta_time,
	                              app->frame_count);
}
