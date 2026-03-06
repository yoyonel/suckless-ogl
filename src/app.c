#include "app.h"

#include "action_notifier.h"
#include "adaptive_sampler.h"
#include "app_binding.h"
#include "app_input.h"
#include "app_settings.h"
#include "app_ui.h"
#include "async_loader.h"
#include "camera.h"
#include "fps.h"
#include "gl_common.h"
#include "glad/glad.h"
#include "perf_mode.h"
#include "postprocess.h"
#include "profiler.h"
#include "renderer.h"
#include "scene.h"
#include "texture.h"
#include "tracy_gpu.h"
#include "ui.h"
#include "window.h"
#include <GLFW/glfw3.h>
#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/util.h>
#include <stb_image.h>
#include <stdlib.h>
#include <string.h>

static const char* const DEFAULT_ENV_FILENAME = "env.hdr";

int app_init(App* app, int width, int height, const char* title)
{
	app->width = width;
	app->height = height;

	app->camera_enabled = true;
	app->is_fullscreen = false;
	app->scene.specular_aa_enabled = DEFAULT_SPECULAR_AA_ENABLED;
	app->env_mgr.is_first_load = true;

	/* Initialize Help UI state */
	app_binding_registry_init(&app->binding_registry);

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

	async_coordinator_init(&app->async_coord);

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
		env_manager_load(&app->env_mgr, app->async_loader,
		                 app->scene.hdr_files[default_idx]);
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
	app_ui_init(&app->overlay);

	if (!postprocess_init(&app->postprocess, &app->gpu_profiler, width,
	                      height)) {
		return 0;
	}
	postprocess_set_dummy_textures(&app->postprocess,
	                               app->scene.dummy_black_tex);
	postprocess_set_exposure(&app->postprocess,
	                         app->postprocess.auto_threshold);
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
	app_ui_cleanup(&app->overlay);
	postprocess_cleanup(&app->postprocess);

	/* Async Loader Shutdown before other resources */
	async_loader_destroy(app->async_loader);
	app->async_loader = NULL;

	/* 2. Scene / Rendering groups */
	scene_cleanup(&app->scene);

	/* 3. Common low-level resources */

	async_coordinator_cleanup(&app->async_coord);

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
			PROFILE_ZONE(poll_ctx, "GLFW PollEvents (Start)");
			glfwPollEvents();
			PROFILE_ZONE_END(poll_ctx);
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

		app_ui_update(&app->overlay, app->delta_time);

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
			PROFILE_ZONE(update_ctx, "App Update");
			app_update(app);
			PROFILE_ZONE_END(update_ctx);
		}

		{
			PROFILE_ZONE(render_ctx, "App Render");
			renderer_draw_frame(
			    app, &app->scene, &app->postprocess, &app->camera,
			    &app->gpu_profiler, &app->timeline_ui,
			    &app->env_mgr, &app->notifier, &app->effect_bench,
			    app->width, app->height, app->delta_time,
			    app->frame_count, app->log_gpu_metrics);
			PROFILE_ZONE_END(render_ctx);
		}

		{
			PROFILE_ZONE(tracy_mark_ctx, "Tracy Mark/Screenshots");
			tracy_manager_update_screenshots(&app->tracy_mgr, app);
			PROFILE_ZONE_END(tracy_mark_ctx);
		}

		{
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
		app->scene.recycled_hdr_tex =
		    texture_preallocate_hdr(app->async_coord.pending_prealloc_w,
		                            app->async_coord.pending_prealloc_h,
		                            app->scene.recycled_hdr_tex);
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
		env_manager_process_loading_step(&app->env_mgr,
		                                 &app->scene.recycled_hdr_tex,
		                                 &app->scene.ibl_coord);
	}

	env_manager_update_ibl(&app->env_mgr, &app->scene, &app->postprocess,
	                       app->frame_count, app->width, app->height);
	env_manager_update_transition(&app->env_mgr, &app->scene,
	                              &app->postprocess, app->delta_time,
	                              app->frame_count);
}
