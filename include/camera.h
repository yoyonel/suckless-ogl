/**
 * @file camera.h
 * @brief First-person camera module with realistic physics and head-bobbing.
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>

/* --- Camera Defaults --- */

#define DEFAULT_CAMERA_SPEED 15.0F        /**< Units per second. */
#define DEFAULT_CAMERA_SENSITIVITY 0.15F  /**< Mouse scaling. */
#define DEFAULT_CAMERA_ZOOM 60.0F         /**< Default FOV. */
#define DEFAULT_ZOOM_SPEED 1.0F           /**< FOV adjustment rate. */
#define DEFAULT_SCROLL_SENSITIVITY 50.0F  /**< Scroll impulsiveness. */
#define DEFAULT_MAX_PITCH 89.0F           /**< Upward look limit. */
#define DEFAULT_MIN_PITCH -89.0F          /**< Downward look limit. */
#define DEFAULT_MAX_ALPHA 1.0F            /**< Unused. */
#define DEFAULT_ACCELERATION 10.0F        /**< Physical push strength. */
#define DEFAULT_FRICTION 0.85F            /**< Velocity decay per frame. */
#define DEFAULT_ROTATION_SMOOTHING 0.18F  /**< Orientation lerp factor. */
#define DEFAULT_BOBBING_FREQUENCY 2.2F    /**< Waves per meter. */
#define DEFAULT_BOBBING_AMPLITUDE 0.0004F /**< Height of head-bob. */
#define DEFAULT_MIN_VELOCITY_FOR_BOBBING \
	0.5F                              /**< threshold to trigger bobbing. */
#define DEFAULT_BOBBING_RESET_SPEED 0.95F /**< Decay of bobbing offset. */
#define DEFAULT_MIN_VELOCITY 0.01F        /**< Cutoff for full stop. */
#define DEFAULT_TARGET_FPS 60             /**< Reference for fixed updates. */
#define DEFAULT_FIXED_TIMESTEP \
	(1.0F / DEFAULT_TARGET_FPS)         /**< Simulation step. */
#define DEFAULT_MOUSE_SMOOTHING_FACTOR 0.1F /**< Input lag simulation. */

/**
 * @struct Camera
 * @brief Represents a 3D camera with orientation, movement, and physical
 * properties.
 */
typedef struct Camera {
	vec3 position; /**< World position. */
	vec3 front;    /**< Front direction vector (normalized). */
	vec3 up;       /**< Up direction vector (normalized). */
	vec3 right;    /**< Right direction vector (normalized). */
	vec3 world_up; /**< World's up direction (usually 0,1,0). */

	float yaw;   /**< Horizontal rotation in degrees. */
	float pitch; /**< Vertical rotation in degrees. */

	float velocity;    /**< Maximum movement speed. */
	float sensitivity; /**< Mouse sensitivity factor. */
	float zoom;        /**< Current Field of View (FOV) in degrees. */

	/* Movement states (booleans — set by keyboard callbacks) */
	int move_forward;
	int move_backward;
	int move_left;
	int move_right;
	int move_up;
	int move_down;

	/* Unified movement input [-1,1] per camera-local axis.
	 * [0] = right (+) / left (-),
	 * [1] = up (+) / down (-),
	 * [2] = forward (+) / backward (-).
	 * Built each physics step from keyboard flags and/or gamepad. */
	vec3 move_input;

	/* Realistic physics */
	vec3 velocity_current; /**< Current 3D velocity vector (momentum). */
	float acceleration;    /**< Speed increase factor. */
	float friction;        /**< Decay factor when no input is provided (0.0
	                          to 1.0). */

	/* Smooth rotation */
	float yaw_target;         /**< Target yaw to lerp towards. */
	float pitch_target;       /**< Target pitch to lerp towards. */
	float rotation_smoothing; /**< Interpolation factor for rotation. */
	float smoothed_x;         /**< Input smoothing state X. */
	float smoothed_y;         /**< Input smoothing state Y. */

	/* Head bobbing */
	float bobbing_time; /**< Accumulated time for the sine-wave oscillation.
	                     */
	float bobbing_frequency; /**< Speed of the bobbing motion. */
	float
	    bobbing_amplitude; /**< Vertical distance of the bobbing motion. */
	int bobbing_enabled;   /**< Boolean toggle for head bobbing effect. */

	/* Timing */
	float physics_accumulator; /**< Residual time for fixed-step physics. */
	float fixed_timestep; /**< Target duration for one physics update. */

	float mouse_smoothing_factor; /**< Input lag simulation factor for
	                                 smoother movement. */

	/* Input State */
	double last_mouse_x; /**< Previous mouse X position. */
	double last_mouse_y; /**< Previous mouse Y position. */
	int first_mouse;     /**< Flag to handle initial mouse jump. */
} Camera;

/**
 * @brief Initializes the camera with default values.
 * @param cam Pointer to the camera instance.
 * @param distance Initial forward offset (legacy).
 * @param yaw Initial horizontal rotation.
 * @param pitch Initial vertical rotation.
 */
void camera_init(Camera* cam, float distance, float yaw, float pitch);

/**
 * @brief Recalculates front, right, and up vectors from yaw and pitch.
 * @param cam Pointer to the camera instance.
 * @note Updates cam->front, cam->right, and cam->up.
 */
void camera_update_vectors(Camera* cam);

/**
 * @brief Processes mouse movement to update target orientation.
 * @param cam Pointer to the camera instance.
 * @param xoffset Relative horizontal mouse movement.
 * @param yoffset Relative vertical mouse movement.
 */
void camera_process_mouse(Camera* cam, float xoffset, float yoffset);

/**
 * @brief Generates the 4x4 view matrix for this camera.
 * @param cam Pointer to the camera instance.
 * @param[out] view Matrix to populate.
 */
void camera_get_view_matrix(Camera* cam, mat4 view);

/**
 * @brief Processes mouse scroll events to apply physical impulse.
 * @param cam Pointer to the camera instance.
 * @param yoffset Scroll amount along the y-axis.
 */
void camera_process_scroll(Camera* cam, float yoffset);

/**
 * @brief Performs one fixed-step physics update.
 *
 * Reads move_input to build target_velocity, then applies momentum,
 * friction, and head-bobbing.  move_input must be set before calling
 * (via camera_build_keyboard_input and/or gamepad overlay).
 * @param cam Pointer to the camera instance.
 */
void camera_fixed_update(Camera* cam);

/**
 * @brief Converts boolean keyboard flags into move_input.
 *
 * Writes cam->move_input from cam->move_forward/backward/left/right/up/down.
 * Values are binary (0 or ±1).  Call before gamepad overlay and
 * camera_fixed_update each physics step.
 * @param cam Pointer to the camera instance.
 */
void camera_build_keyboard_input(Camera* cam);

#endif /* CAMERA_H */
