#ifndef RENDERER_H
#define RENDERER_H

#include "action_notifier.h"
#include "camera.h"
#include "effect_benchmark.h"
#include "env_manager.h"
#include "gpu_profiler.h"
#include "gpu_profiler_ui.h"
#include "postprocess.h"
#include "scene.h"
#include "ui.h"
#include <stdint.h>

/**
 * @brief Callback type for rendering the UI overlay.
 * Decouples the renderer from the App struct.
 */
typedef void (*RenderUIFn)(void* user_data);

/**
 * @brief All state needed by renderer_draw_frame for a single frame.
 * Populated by the caller (App or test harness) once per frame.
 */
typedef struct RenderContext {
	Scene* scene;
	PostProcess* postprocess;
	Camera* camera;
	GPUProfiler* profiler;
	GPUProfilerUI* profiler_ui;
	EnvManager* env_mgr;
	ActionNotifier* notifier;
	EffectBenchmark* effect_bench;
	int width;
	int height;
	double delta_time;
	uint64_t frame_count;
	int log_gpu_metrics;
	RenderUIFn render_ui;
	void* render_ui_data;
} RenderContext;

/**
 * @brief Orchestrates the entire frame render (scene, postprocess, ui).
 */
void renderer_draw_frame(const RenderContext* ctx);

#endif /* RENDERER_H */
