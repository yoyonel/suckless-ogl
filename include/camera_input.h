#ifndef CAMERA_INPUT_H
#define CAMERA_INPUT_H

/* Forward declaration */
typedef struct Camera Camera;

/**
 * @file camera_input.h
 * @brief Input handling adapter for the Camera system.
 *
 * This module bridges raw input events (keys, mouse, scroll) to the
 * Camera's internal state and logic. It decouples the input source
 * (e.g., GLFW) from the Camera implementation.
 */

/**
 * @brief Processes a keyboard event to update camera movement flags.
 *
 * Maps specific keys (W, A, S, D, Q, E) to camera movement directions.
 *
 * @param cam Pointer to the Camera instance.
 * @param key The key code (e.g., GLFW_KEY_W).
 * @param action The key action (e.g., GLFW_PRESS, GLFW_RELEASE).
 */
void camera_input_handle_key(Camera* cam, int key, int action);

/**
 * @brief Processes a mouse movement event to update camera orientation.
 *
 * Calculates the delta from the last known position stored in the Camera
 * struct and applies rotation (yaw/pitch).
 *
 * @param cam Pointer to the Camera instance.
 * @param xpos The current absolute X coordinate of the mouse.
 * @param ypos The current absolute Y coordinate of the mouse.
 */
void camera_input_handle_mouse(Camera* cam, double xpos, double ypos);

/**
 * @brief Processes a scroll event to adjust camera speed or zoom.
 *
 * @param cam Pointer to the Camera instance.
 * @param yoffset The vertical scroll amount.
 */
void camera_input_handle_scroll(Camera* cam, double yoffset);

#endif /* CAMERA_INPUT_H */
