/**
 * @file gamepad_input.h
 * @brief Gamepad/controller input adapter for camera control.
 *
 * Polls GLFW gamepad state and translates stick axes and buttons
 * into camera movement and orientation, with configurable dead-zones.
 * Designed for DualShock 4 / DualSense but works with any
 * SDL_GameControllerDB-mapped controller.
 */

#ifndef GAMEPAD_INPUT_H
#define GAMEPAD_INPUT_H

typedef struct Camera Camera;

/** Maximum number of gamepad buttons tracked for edge detection. */
#define GAMEPAD_BUTTON_COUNT 15

/** Default dead-zone threshold for analog sticks (0.0–1.0). */
#define GAMEPAD_DEFAULT_DEADZONE 0.15F

/** Look speed for the right stick in degrees per second. */
#define GAMEPAD_DEFAULT_LOOK_SENSITIVITY 120.0F

/** Sensitivity multiplier for the left stick (movement). */
#define GAMEPAD_DEFAULT_MOVE_SENSITIVITY 1.0F

/** Trigger threshold to register as "pressed" (0.0–1.0). */
#define GAMEPAD_DEFAULT_TRIGGER_THRESHOLD 0.1F

/**
 * @struct GamepadActions
 * @brief Edge-detected button events produced by a single poll.
 */
typedef struct GamepadActions {
	int env_next; /**< R1 pressed → cycle to next environment map. */
	int env_prev; /**< L1 pressed → cycle to previous environment map. */
} GamepadActions;

/** Number of gamepad axes cached for per-step application. */
#define GAMEPAD_AXIS_COUNT 6

typedef struct GamepadState {
	int connected;          /**< Non-zero if a gamepad is present. */
	int joystick_id;        /**< GLFW joystick slot (GLFW_JOYSTICK_1…16). */
	float deadzone;         /**< Analog stick dead-zone threshold. */
	float look_sensitivity; /**< Right-stick sensitivity multiplier. */
	float move_sensitivity; /**< Left-stick sensitivity multiplier. */
	float trigger_threshold; /**< Trigger activation threshold. */
	unsigned char prev_buttons[GAMEPAD_BUTTON_COUNT]; /**< Previous frame
	                                                     button state. */
	float axes[GAMEPAD_AXIS_COUNT]; /**< Deadzone-filtered axis snapshot. */
} GamepadState;

/**
 * @brief Initializes gamepad state with default values.
 * @param state Pointer to the gamepad state to initialize.
 */
void gamepad_input_init(GamepadState* state);

/**
 * @brief Polls GLFW gamepad state and detects button events.
 *
 * Reads axes (with deadzone) into state->axes for later use by
 * gamepad_write_input.  Detects edge-triggered button presses.
 * Call once per frame, before the physics accumulator loop.
 *
 * @param state   Pointer to the persistent gamepad state.
 * @param actions Output struct for edge-detected button events (may be NULL).
 */
void gamepad_input_poll(GamepadState* state, GamepadActions* actions);

/**
 * @brief Writes gamepad axes into cam->move_input and applies look.
 *
 * Overlays analog stick values onto the camera's unified move_input.
 * Must be called after camera_build_keyboard_input and before
 * camera_fixed_update each physics step.
 *
 * @param state Pointer to the gamepad state (with cached axes).
 * @param cam   Pointer to the camera to drive.
 */
void gamepad_write_input(const GamepadState* state, Camera* cam);

/**
 * @brief Applies a dead-zone filter to an axis value.
 *
 * Returns 0.0 if |value| < deadzone, otherwise rescales linearly
 * so the usable range is [0, 1] (preserving sign).
 *
 * @param value    Raw axis value in [-1, 1].
 * @param deadzone Dead-zone threshold in [0, 1).
 * @return Filtered axis value.
 */
float gamepad_apply_deadzone(float value, float deadzone);

#endif /* GAMEPAD_INPUT_H */
