#ifndef APP_H
#define APP_H

#include "action_notifier.h"   // IWYU pragma: export
#include "app_ui.h"            // IWYU pragma: export
#include "app_window.h"        // IWYU pragma: export
#include "effect_benchmark.h"  // IWYU pragma: export
#include "gl_common.h"         // IWYU pragma: export
#include "gui.h"
#include <cglm/cglm.h>  // IWYU pragma: export

/* --- Opaque sub-struct forward declarations --- */
typedef struct AppProfiling AppProfiling;
typedef struct AppInput AppInput;
typedef struct PostProcess PostProcess;
typedef struct Scene Scene;
typedef struct EnvManager EnvManager;
typedef struct AsyncLoader AsyncLoader;
typedef struct AsyncCoordinator AsyncCoordinator;

/**
 * @struct App
 * @brief The central state container for the entire application.
 */
typedef struct App {
	/* --- Pointers and Dynamic Objects --- */
	Scene* scene;             /**< The 3D scene (Includes GI Probe Grid). */
	PostProcess* postprocess; /**< Main post-processing pipeline. */
	AppWindow win; /**< Window handle, fullscreen & resize state. */
	double last_frame_time;      /**< Absolute time of last frame start. */
	double delta_time;           /**< Time elapsed since last frame. */
	uint64_t frame_count;        /**< Monotonic frame counter. */
	float* lum_histogram_buffer; /**< Pre-allocated buffer for histogram. */

	/* --- Sub-Modules (Opaque, Heap-Allocated) --- */
	AppProfiling* profiling; /**< Profiling and metrics sub-system. */
	AppInput* input;      /**< Camera, gamepad, key-bindings sub-system. */
	AppUIOverlay overlay; /**< Overlay and text rendering state. */
	Gui_C* imgui;         /**< ImGui GUI interface context. */

	/* --- App State Flags and Values --- */
	const char* title;       /**< Window title (borrowed, not owned). */
	int width;               /**< Current window/viewport width. */
	int height;              /**< Current window/viewport height. */
	EnvManager* env_mgr;     /**< Environment/IBL state. */
	ActionNotifier notifier; /**< Temporary user notifications. */
	EffectBenchmark effect_bench; /**< A/B effect cost measurement. */

	/* --- Global GPU Resources --- */
	GLuint lum_ssbo[2]; /**< Double-buffered storage for luminance. */

	/* --- Global Configuration Uniforms --- */
	float u_metallic;  /**< Override metallic for all objects. */
	float u_roughness; /**< Override roughness for all objects. */
	float u_ao;        /**< Override AO for all objects. */
	float u_exposure;  /**< Manual exposure compensation. */

	AsyncLoader* async_loader; /**< Background asset loader context. */
	AsyncCoordinator*
	    async_coord; /**< Manages PBO allocation & async synchronization. */

} App;

/* --- Core Control Flow --- */

/**
 * @brief Fully initializes the application state, window, and OpenGL context.
 */
int app_init(App* app, int width, int height, const char* title);

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

/**
 * @brief Sets the GUI visibility state and synchronizes cursor/camera state.
 */
void app_set_gui_visible(App* app, bool visible);

/**
 * @brief Toggles the GUI visibility state.
 */
void app_toggle_gui(App* app);

enum { TRACY_SCREENSHOT_WIDTH = 320, TRACY_SCREENSHOT_HEIGHT = 180 };

#endif /* APP_H */
