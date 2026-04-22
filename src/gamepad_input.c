#include "gamepad_input.h"

#include "camera.h"
#include "log.h"
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <math.h>

/** Factor to convert GLFW trigger range [-1,1] to normalised [0,1]. */
#define TRIGGER_NORM_FACTOR 0.5F

void gamepad_input_init(GamepadState* state)
{
	state->connected = 0;
	state->joystick_id = GLFW_JOYSTICK_1;
	state->deadzone = GAMEPAD_DEFAULT_DEADZONE;
	state->look_sensitivity = GAMEPAD_DEFAULT_LOOK_SENSITIVITY;
	state->move_sensitivity = GAMEPAD_DEFAULT_MOVE_SENSITIVITY;
	state->trigger_threshold = GAMEPAD_DEFAULT_TRIGGER_THRESHOLD;
	for (int idx = 0; idx < GAMEPAD_BUTTON_COUNT; idx++) {
		state->prev_buttons[idx] = 0;
	}
	for (int idx = 0; idx < GAMEPAD_AXIS_COUNT; idx++) {
		state->axes[idx] = 0.0F;
	}
}

float gamepad_apply_deadzone(float value, float deadzone)
{
	if (fabsf(value) < deadzone) {
		return 0.0F;
	}
	/* Rescale so usable range starts at 0 after the dead-zone. */
	float sign = (value > 0.0F) ? 1.0F : -1.0F;
	return sign * (fabsf(value) - deadzone) / (1.0F - deadzone);
}

/**
 * @brief Detects edge-triggered button presses and saves state.
 */
static void gamepad_poll_buttons(GamepadState* state,
                                 const GLFWgamepadstate* pad,
                                 GamepadActions* actions)
{
	if (actions) {
		unsigned char r1_now =
		    pad->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER];
		unsigned char l1_now =
		    pad->buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER];
		unsigned char r1_prev =
		    state->prev_buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER];
		unsigned char l1_prev =
		    state->prev_buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER];

		if (r1_now && !r1_prev) {
			actions->env_next = 1;
		}
		if (l1_now && !l1_prev) {
			actions->env_prev = 1;
		}

		unsigned char share_now =
		    pad->buttons[GLFW_GAMEPAD_BUTTON_BACK];
		unsigned char share_prev =
		    state->prev_buttons[GLFW_GAMEPAD_BUTTON_BACK];
		if (share_now && !share_prev) {
			actions->camera_reset = 1;
		}
	}
	/* Save button state for next-frame edge detection. */
	for (int idx = 0; idx < GAMEPAD_BUTTON_COUNT; idx++) {
		state->prev_buttons[idx] = pad->buttons[idx];
	}
}

void gamepad_input_poll(GamepadState* state, GamepadActions* actions)
{
	/* Detect connection. */
	int was_connected = state->connected;
	state->connected = glfwJoystickIsGamepad(state->joystick_id);

	if (actions) {
		actions->env_next = 0;
		actions->env_prev = 0;
		actions->camera_reset = 0;
	}

	if (state->connected && !was_connected) {
		const char* name = glfwGetGamepadName(state->joystick_id);
		LOG_INFO("suckless-ogl.gamepad", "Gamepad connected: %s",
		         name ? name : "Unknown");
	} else if (!state->connected && was_connected) {
		LOG_INFO("suckless-ogl.gamepad", "Gamepad disconnected");
	}

	if (!state->connected) {
		for (int idx = 0; idx < GAMEPAD_AXIS_COUNT; idx++) {
			state->axes[idx] = 0.0F;
		}
		return;
	}

	GLFWgamepadstate pad;
	if (!glfwGetGamepadState(state->joystick_id, &pad)) {
		state->connected = 0;
		for (int idx = 0; idx < GAMEPAD_AXIS_COUNT; idx++) {
			state->axes[idx] = 0.0F;
		}
		return;
	}

	/* Cache deadzone-filtered axes for gamepad_apply_movement. */
	state->axes[GLFW_GAMEPAD_AXIS_LEFT_X] = gamepad_apply_deadzone(
	    pad.axes[GLFW_GAMEPAD_AXIS_LEFT_X], state->deadzone);
	state->axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = gamepad_apply_deadzone(
	    pad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], state->deadzone);
	state->axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = gamepad_apply_deadzone(
	    pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], state->deadzone);
	state->axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = gamepad_apply_deadzone(
	    pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], state->deadzone);

	/* Normalise triggers from [-1,1] to [0,1]. */
	float trig_left = (pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0F) *
	                  TRIGGER_NORM_FACTOR;
	float trig_right = (pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0F) *
	                   TRIGGER_NORM_FACTOR;
	state->axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] =
	    (trig_left > state->trigger_threshold) ? trig_left : 0.0F;
	state->axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] =
	    (trig_right > state->trigger_threshold) ? trig_right : 0.0F;

	/* Edge-detect button presses. */
	gamepad_poll_buttons(state, &pad, actions);
}

void gamepad_write_input(const GamepadState* state, Camera* cam)
{
	if (!state->connected) {
		return;
	}

	/* Left stick → move_input (overrides keyboard if stick has input).
	 * stick_lx maps to move_input[0] (right/left),
	 * stick_ly maps to move_input[2] (forward/back, inverted). */
	float stick_lx = state->axes[GLFW_GAMEPAD_AXIS_LEFT_X];
	float stick_ly = state->axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

	if (fabsf(stick_lx) > 0.0F) {
		cam->move_input[0] = stick_lx * state->move_sensitivity;
	}
	if (fabsf(stick_ly) > 0.0F) {
		cam->move_input[2] = -stick_ly * state->move_sensitivity;
	}

	/* Triggers → move_input[1] (up/down).
	 * R2 = up, L2 = down. */
	float trig_right = state->axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
	float trig_left = state->axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];

	if (trig_right > 0.0F || trig_left > 0.0F) {
		cam->move_input[1] =
		    (trig_right - trig_left) * state->move_sensitivity;
	}

	/* Right stick → Look (yaw / pitch in degrees/sec). */
	float stick_rx = state->axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
	float stick_ry = state->axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];

	if (fabsf(stick_rx) > 0.0F || fabsf(stick_ry) > 0.0F) {
		float look_speed =
		    state->look_sensitivity * cam->fixed_timestep;
		cam->yaw_target += stick_rx * look_speed;
		cam->pitch_target -= stick_ry * look_speed;

		if (cam->pitch_target > DEFAULT_MAX_PITCH) {
			cam->pitch_target = DEFAULT_MAX_PITCH;
		}
		if (cam->pitch_target < DEFAULT_MIN_PITCH) {
			cam->pitch_target = DEFAULT_MIN_PITCH;
		}
	}
}
