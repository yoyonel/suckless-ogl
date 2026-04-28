/**
 * @file gamepad_context.h
 * @brief Decoupling seam between gamepad input and camera internals.
 *
 * GamepadContext exposes only the minimal subset of camera state that
 * gamepad_write_input() needs, breaking the direct dependency on camera.h.
 */

#ifndef GAMEPAD_CONTEXT_H
#define GAMEPAD_CONTEXT_H

/**
 * @struct GamepadContext
 * @brief Minimal camera state slice for gamepad input application.
 *
 * The caller fills this from Camera fields before calling
 * gamepad_write_input(), then copies the results back.
 */
typedef struct GamepadContext {
	float move_input[3];  /**< [right/left, up/down, fwd/back]. */
	float yaw_target;     /**< Target yaw in degrees. */
	float pitch_target;   /**< Target pitch in degrees. */
	float fixed_timestep; /**< Physics step duration (seconds). */
	float max_pitch;      /**< Upper pitch clamp (degrees). */
	float min_pitch;      /**< Lower pitch clamp (degrees). */
} GamepadContext;

#endif /* GAMEPAD_CONTEXT_H */
