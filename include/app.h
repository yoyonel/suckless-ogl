#ifndef APP_H
#define APP_H

#include "action_notifier.h"
#include "adaptive_sampler.h"
#include "async_loader.h"
#include "camera.h"
#include "effect_benchmark.h"
#include "env_manager.h"
#include "fps.h"
#include "gl_common.h"
#include "gpu_profiler.h"
#include "gpu_profiler_ui.h"
#include "perf_mode.h"
#include "postprocess.h"
#include "scene.h"
#include "tracy_manager.h"
#include "ui.h"
#include "rhi.h"
#include <cglm/cglm.h>

#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif

typedef struct AsyncLoader
    AsyncLoader; /**< Forward declaration of AsyncLoader. */

/**
 * @struct App
 * @brief The central state container for the entire application.
 */
typedef struct App {
	/* --- Pointers and Dynamic Objects --- */
	Scene scene;             /**< The 3D scene (Includes GI Probe Grid). */
	PostProcess postprocess; /**< Main post-processing pipeline. */
	GLFWwindow* window;      /**< The GLFW window context. */
	double last_frame_time;  /**< Absolute time of last frame start. */
	double delta_time;       /**< Time elapsed since last frame. */
	uint64_t frame_count;    /**< Monotonic frame counter. */
	float* lum_histogram_buffer; /**< Pre-allocated buffer for histogram. */

	/* --- Sub-Modules (RAII/In-Place) --- */
	FpsCounter fps_counter;      /**< Rolling average FPS manager. */
	AdaptiveSampler fps_sampler; /**< Jitter compensation for input. */
	GPUProfiler gpu_profiler;
	GPUProfilerUI timeline_ui;
	UIContext ui; /**< Overlay and text rendering state. */

	Camera camera; /**< View/Proj state. */

	/* --- App State Flags and Values --- */
	int width;                     /**< Current window/viewport width. */
	int height;                    /**< Current window/viewport height. */
	int is_fullscreen;             /**< Fullscreen toggle state. */
	int show_exposure_debug;       /**< Enable auto-exposure histogram. */
	int show_help;                 /**< Overlay help text. */
	int show_info_overlay;         /**< Show FPS and stats. */
	int text_overlay_mode;         /**< Verbosity level of text UI. */
	int saved_x, saved_y;          /**< Cached pos for window restore. */
	int saved_width, saved_height; /**< Cached size for window restore. */
	int camera_enabled;            /**< Pause camera movement. */
	EnvManager env_mgr;            /**< Environment/IBL state. */
	int perf_mode_active; /**< Performance/GameMode optimization active. */
	PerfModeContext perf_context; /**< Performance mode state context. */
	ActionNotifier notifier;      /**< Temporary user notifications. */
	EffectBenchmark effect_bench; /**< A/B effect cost measurement. */
	int log_gpu_metrics; /**< Toggle console logging of GPU stats. */

	/* --- Global GPU Resources --- */
	GLuint upload_pbo[2];
	int upload_pbo_idx;
	GLsizeiptr upload_pbo_size[2];
	int pending_prealloc_w; /**< Deferred pre-alloc width (0=none). */
	int pending_prealloc_h; /**< Deferred pre-alloc height. */

	GLuint exposure_pbo; /**< Pixel Buffer Object for mean luma readback. */
	GLuint histogram_pbo;   /**< Pixel Buffer Object for luminance histogram
	                           readback. */
	GLuint lum_ssbo[2];     /**< Double-buffered storage for luminance. */
	TracyManager tracy_mgr; /**< Tracy instrumentation manager. */

	/* --- Global Configuration Uniforms --- */
	float u_metallic;       /**< Override metallic for all objects. */
	float u_roughness;      /**< Override roughness for all objects. */
	float u_ao;             /**< Override AO for all objects. */
	float u_exposure;       /**< Manual exposure compensation. */
	float auto_threshold;   /**< Dynamic exposure target. */
	float current_exposure; /**< Integrated GPU exposure value. */

	AsyncLoader* async_loader; /**< Background asset loader context. */

	RHIContext rhi;
} App;

/* --- Core Control Flow --- */

/**
 * @brief Fully initializes the application state, window, and OpenGL context.
 */
int app_init(App* app, int width, int height, const char* title, GraphicsAPI api);

/**
 * @brief Safely releases all GPU and CPU resources held by the application.
 */
void app_cleanup(App* app);

/**
 * @brief Enters the main application rendering and event loop.
 */
void app_run(App* app);

/**
 * @brief One-frame logic update (physics, timers, camera).
 */
void app_update(App* app);

#include "app_input.h"
#include "app_ui.h"

enum { TRACY_SCREENSHOT_WIDTH = 320, TRACY_SCREENSHOT_HEIGHT = 180 };

#endif /* APP_H */
