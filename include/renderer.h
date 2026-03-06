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

/* Forward declare App UI function */
struct App;
void app_render_ui(const struct App* app);

/**
 * @brief Orchestrates the entire frame render (scene, postprocess, ui).
 * Decouples the rendering logic from the main App lifecycle and window state.
 */
void renderer_draw_frame(struct App* app_ref, Scene* scene,
                         PostProcess* postprocess, Camera* camera,
                         GPUProfiler* profiler, GPUProfilerUI* timeline_ui,
                         EnvManager* env_mgr, ActionNotifier* notifier,
                         EffectBenchmark* effect_bench, int width, int height,
                         double delta_time, uint64_t frame_count,
                         int log_gpu_metrics);

#endif /* RENDERER_H */
