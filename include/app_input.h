/**
 * @file app_input.h
 * @brief User input orchestration and application state toggles.
 *
 * This module manages all high-level input logic, bridging GLFW raw events
 * to application-specific actions like post-processing toggles, environment
 * map switching, and camera movement.
 *
 * Input handlers receive an AppInputContext (focused pointer bundle) instead
 * of the full App struct, following the same pattern as PostProcessInputContext
 * and camera_input.
 */

#ifndef APP_INPUT_H
#define APP_INPUT_H

#include "gl_common.h"

/* Forward declarations — avoids pulling heavy headers into app_input.h */
typedef struct App App;
typedef struct Camera Camera;
typedef struct Scene Scene;
typedef struct PostProcess PostProcess;
typedef struct EnvManager EnvManager;
typedef struct ActionNotifier ActionNotifier;
typedef struct AppUIOverlay AppUIOverlay;
typedef struct GPUProfilerUI GPUProfilerUI;
typedef struct EffectBenchmark EffectBenchmark;
typedef struct PerfModeContext PerfModeContext;
typedef struct GamepadState GamepadState;
typedef struct AsyncLoader AsyncLoader;

/**
 * @struct AppInputContext
 * @brief Focused context for application-level input handling.
 *
 * Decouples input logic from the App God Object by exposing only the
 * fields that input handlers actually need. Constructed once per frame
 * (or per callback) from App fields in app.c.
 */
typedef struct {
	GLFWwindow* window;
	Camera* camera;
	Scene* scene;
	PostProcess* postprocess;
	EnvManager* env_mgr;
	ActionNotifier* notifier;
	AppUIOverlay* overlay;
	GPUProfilerUI* timeline_ui;
	EffectBenchmark* effect_bench;
	PerfModeContext* perf_context;
	GamepadState* gamepad;
	AsyncLoader* async_loader;

	/* Mutable scalar state (pointers so mutations propagate to App) */
	int* width;
	int* height;
	int* camera_enabled;
	int* is_fullscreen;
	int* saved_x;
	int* saved_y;
	int* saved_width;
	int* saved_height;
	int* resize_pending;
	int* pending_width;
	int* pending_height;
	int* perf_mode_active;
	int* log_gpu_metrics;
} AppInputContext;

/**
 * @brief Primary GLFW key callback.
 *
 * Dispatches raw key events to `handle_app_input`,
 * `camera_process_key_callback`, and other specific handlers.
 * @param window The GLFW window context.
 * @param key The keyboard key code.
 * @param scancode System-specific scancode.
 * @param action GLFW_PRESS, GLFW_RELEASE, or GLFW_REPEAT.
 * @param mods Active modifier keys (Shift, Ctrl, Alt).
 */
void key_callback(GLFWwindow* window, int key, int scancode, int action,
                  int mods);

/**
 * @brief Primary GLFW mouse position callback.
 * @param window The GLFW window context.
 * @param xpos Absolute mouse X-coordinate.
 * @param ypos Absolute mouse Y-coordinate.
 */
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

/**
 * @brief Primary GLFW scroll callback.
 * @param window The GLFW window context.
 * @param xoffset Scroll amount along the X-axis.
 * @param yoffset Scroll amount along the Y-axis.
 */
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

/**
 * @brief Primary GLFW framebuffer size callback.
 *
 * Handles resizing of the OpenGL viewport and all dependent post-processing
 * buffers to match the new window dimensions.
 * @param window The GLFW window context.
 * @param width New framebuffer width.
 * @param height New framebuffer height.
 * @note Reallocates many GPU textures via `postprocess_resize`.
 */
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

/**
 * @brief Dispatches application-level logic for key inputs.
 *
 * Handles system-wide shortcuts (ESC to exit, Space to pause, etc.).
 * @param ctx Pointer to the input context.
 * @param key Locked key code.
 * @param mods Active modifiers.
 */
void handle_app_input(AppInputContext* ctx, int key, int mods);

/* --- Internal Logic Bridge Functions --- */

/**
 * @brief Handles input for cycling environment maps.
 * @param ctx Pointer to the input context.
 * @param action GLFW action (Press/Release).
 * @param mods Modifiers.
 * @param key Directional key.
 */
void app_handle_env_input(AppInputContext* ctx, int action, int mods, int key);

/**
 * @brief Toggles the application window between Windowed and Fullscreen.
 * @param ctx Pointer to the input context.
 * @param window GLFW window handle.
 */
void app_toggle_fullscreen(AppInputContext* ctx, GLFWwindow* window);

/**
 * @brief Captures the current framebuffer and saves it as a PNG file.
 * @param ctx Pointer to the input context.
 * @param filename Output file path (should end in .png).
 */
void app_save_png_frame(AppInputContext* ctx, const char* filename);

/**
 * @brief Constructs an AppInputContext from the full App state.
 *
 * Bridges between the GLFW user-pointer (App*) and the decoupled
 * input API (AppInputContext*). Defined in app.c to keep the heavy
 * App sub-struct includes out of app_input.c.
 */
AppInputContext app_input_ctx_from_app(App* app);

#endif /* APP_INPUT_H */
