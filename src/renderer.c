#include "renderer.h"

#include "action_notifier.h"
#include "camera.h"
#include "effect_benchmark.h"
#include "env_manager.h"
#include "gl_common.h"
#include "gl_debug.h"
#include "gpu_profiler.h"
#include "gpu_profiler_ui.h"
#include "postprocess_internal.h"
#include "profiler.h"
#include "scene.h"
#include "scene_visuals.h"
#include <GLFW/glfw3.h>

void renderer_draw_frame(const RenderContext* ctx)
{
	bool profiling_enabled =
	    (ctx->profiler_ui->visible || ctx->log_gpu_metrics != 0 ||
	     effect_benchmark_is_running(ctx->effect_bench)) != 0;
	postprocess_update_readbacks(ctx->postprocess, ctx->frame_count);

	PROFILE_FRAME_MARK;

	/* Effect benchmark: read previous frame's profiler results */
	if (effect_benchmark_update(ctx->effect_bench)) {
		action_notifier_push(ctx->notifier,
		                     "FX Benchmark: Done (see log)",
		                     NOTIF_DUR_LONG);
	}

	gl_debug_push_group("Render_Frame");

	postprocess_begin(ctx->postprocess);
	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);

	mat4 view;
	mat4 proj;
	mat4 view_proj;
	mat4 inv_view_proj;
	vec3 camera_pos = {ctx->camera->position[0], ctx->camera->position[1],
	                   ctx->camera->position[2]};
	camera_get_view_matrix(ctx->camera, view);
	if (ctx->height > 0) {
		glm_perspective(glm_rad(ctx->camera->zoom),
		                (float)ctx->width / (float)ctx->height,
		                NEAR_PLANE, FAR_PLANE, proj);
	} else {
		glm_mat4_identity(proj);
	}
	glm_mat4_mul(proj, view, view_proj);
	glm_mat4_inv(view_proj, inv_view_proj);

	/* Provide scene FBO color handle so the shockwave grab pass can use
	 * glCopyImageSubData (texture-to-texture DMA, no framebuffer read). */
	ctx->scene->visuals->shockwave_renderer.scene_color_tex =
	    ctx->postprocess->gpu.scene_color_tex;

	{
		GPU_STAGE_PROFILER(ctx->profiler, "Scene Render",
		                   GPU_PROFILER_SCENE_COLOR);
		gl_debug_push_group("Scene_Render");
		scene_render(
		    ctx->scene, ctx->profiler, view, proj, camera_pos,
		    ctx->postprocess->motion_blur_fx.previous_view_proj,
		    ctx->width, ctx->height);
		gl_debug_pop_group();
	}

	gl_debug_push_group("Post_Processing");
	postprocess_end(ctx->postprocess);
	gl_debug_pop_group();

	postprocess_update_matrices(ctx->postprocess, view_proj);

	{
		GPU_STAGE_PROFILER(ctx->profiler, "UI Overlay",
		                   GPU_PROFILER_UI_COLOR);

		gl_debug_push_group("UI_Overlay");

		/* Render Transition Overlay */
		env_manager_render_overlay(ctx->env_mgr, ctx->scene);

		if (ctx->render_ui) {
			ctx->render_ui(ctx->render_ui_data);
		}

		gl_debug_pop_group();
	}

	gl_debug_pop_group(); /* Render_Frame */

	// 4. Logique d'affichage et animations
	double current_time = glfwGetTime();
	gpu_profiler_ui_update(ctx->profiler_ui, ctx->profiler, ctx->delta_time,
	                       current_time, (bool)ctx->log_gpu_metrics);
}
