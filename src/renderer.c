#include "renderer.h"

#include "app_ui.h"
#include "gl_common.h"
#include "gpu_profiler.h"
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

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
	gpu_profiler_set_enabled(profiler, profiling_enabled);
	gpu_profiler_begin_frame(profiler, frame_count);

#ifdef TRACY_ENABLE
	TracyCFrameMark;
#endif

	/* Effect benchmark: read previous frame's profiler results */
	if (effect_benchmark_update(effect_bench)) {
		action_notifier_push(notifier, "FX Benchmark: Done (see log)",
		                     NOTIF_DUR_LONG);
	}

	// 2. Démarrer la mesure globale de la frame
	GPU_STAGE_PROFILER(profiler, "Total Frame",
	                   GPU_PROFILER_TOTAL_FRAME_COLOR);

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

	scene_render(scene, view, proj, camera_pos,
	             postprocess->motion_blur_fx.previous_view_proj, width,
	             height);

	postprocess_end(postprocess);

	postprocess_update_matrices(postprocess, view_proj);

	{
		GPU_STAGE_PROFILER(profiler, "UI Overlay",
		                   GPU_PROFILER_UI_COLOR);

		/* Render Transition Overlay */
		env_manager_render_overlay(env_mgr, scene);

		app_render_ui(app_ref);
	}

	// 4. Logique d'affichage et animations
	double current_time = glfwGetTime();
	gpu_profiler_ui_update(timeline_ui, profiler, delta_time, current_time,
	                       (bool)log_gpu_metrics);
}
