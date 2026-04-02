#include "renderer.h"

#include "app_ui.h"
#include "gl_common.h"
#include "gl_debug.h"
#include "gpu_profiler.h"
#include "profiler.h"
#include <GLFW/glfw3.h>

void renderer_draw_frame(struct App* app_ref, Scene* scene,
                         PostProcess* postprocess, Camera* camera,
                         GPUProfiler* profiler, GPUProfilerUI* timeline_ui,
                         EnvManager* env_mgr, ActionNotifier* notifier,
                         EffectBenchmark* effect_bench, int width, int height,
                         double delta_time, uint64_t frame_count,
                         int log_gpu_metrics)
{
	bool profiling_enabled =
	    (timeline_ui->visible || log_gpu_metrics != 0 ||
	     effect_benchmark_is_running(effect_bench)) != 0;
	postprocess_update_readbacks(postprocess, frame_count);

	PROFILE_FRAME_MARK;

	/* Effect benchmark: read previous frame's profiler results */
	if (effect_benchmark_update(effect_bench)) {
		action_notifier_push(notifier, "FX Benchmark: Done (see log)",
		                     NOTIF_DUR_LONG);
	}

	gl_debug_push_group("Render_Frame");

	postprocess_begin(postprocess);
	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);

	mat4 view;
	mat4 proj;
	mat4 view_proj;
	mat4 inv_view_proj;
	vec3 camera_pos = {camera->position[0], camera->position[1],
	                   camera->position[2]};
	camera_get_view_matrix(camera, view);
	if (height > 0) {
		glm_perspective(glm_rad(camera->zoom),
		                (float)width / (float)height, NEAR_PLANE,
		                FAR_PLANE, proj);
	} else {
		glm_mat4_identity(proj);
	}
	glm_mat4_mul(proj, view, view_proj);
	glm_mat4_inv(view_proj, inv_view_proj);

	{
		GPU_STAGE_PROFILER(profiler, "Scene Render",
		                   GPU_PROFILER_SCENE_COLOR);
		gl_debug_push_group("Scene_Render");
		scene_render(scene, profiler, view, proj, camera_pos,
		             postprocess->motion_blur_fx.previous_view_proj,
		             width, height);
		gl_debug_pop_group();
	}

	gl_debug_push_group("Post_Processing");
	postprocess_end(postprocess);
	gl_debug_pop_group();

	postprocess_update_matrices(postprocess, view_proj);

	{
		GPU_STAGE_PROFILER(profiler, "UI Overlay",
		                   GPU_PROFILER_UI_COLOR);

		gl_debug_push_group("UI_Overlay");

		/* Render Transition Overlay */
		env_manager_render_overlay(env_mgr, scene);

		app_render_ui(app_ref);

		gl_debug_pop_group();
	}

	gl_debug_pop_group(); /* Render_Frame */

	// 4. Logique d'affichage et animations
	double current_time = glfwGetTime();
	gpu_profiler_ui_update(timeline_ui, profiler, delta_time, current_time,
	                       (bool)log_gpu_metrics);
}
