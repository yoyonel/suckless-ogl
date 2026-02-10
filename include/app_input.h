/**
 * @file app_input.h
 * @brief User input orchestration and application state toggles.
 *
 * This module manages all high-level input logic, bridging GLFW raw events
 * to application-specific actions like post-processing toggles, environment
 * map switching, and camera movement.
 */

#ifndef APP_INPUT_H
#define APP_INPUT_H

typedef struct App App;
typedef struct Camera Camera;
#include "gl_common.h"

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
 * @param app Pointer to the application state.
 * @param key Locked key code.
 * @param mods Active modifiers.
 */
void handle_app_input(App* app, int key, int mods);

/* --- Internal Logic Bridge Functions --- */

/**
 * @brief Handles input for switching post-processing presets.
 * @param app Pointer to the application state.
 * @param key Numeric key code (0-9).
 */
void handle_preset_input(App* app, int key);

/**
 * @brief Handles input for toggling individual effects (Vignette, Bloom, etc.).
 * @param app Pointer to the application state.
 * @param key Function key or shortcut.
 */
void handle_postprocess_input(App* app, int key);

/**
 * @brief Handles input for cycling environment maps.
 * @param app Pointer to the application state.
 * @param action GLFW action (Press/Release).
 * @param mods Modifiers.
 * @param key Directional key.
 */
void app_handle_env_input(App* app, int action, int mods, int key);

/**
 * @brief Bridges GLFW key actions to the camera movement state machine.
 * @param camera Pointer to the camera instance.
 * @param key Key code.
 * @param action Press/Release action.
 */
void camera_process_key_callback(Camera* camera, int key, int action);

/**
 * @brief Toggles the application window between Windowed and Fullscreen.
 * @param app Pointer to the application state.
 * @param window GLFW window handle.
 */
void app_toggle_fullscreen(App* app, GLFWwindow* window);

/**
 * @brief Captures the current framebuffer and saves it as a PNG file.
 * @param app Pointer to the application state.
 * @param filename Output file path (should end in .png).
 */
void app_save_png_frame(App* app, const char* filename);

#endif /* APP_INPUT_H */
