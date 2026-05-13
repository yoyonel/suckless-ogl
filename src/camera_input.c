#include "camera_input.h"

#include "camera.h"
#include "gamepad_input.h"
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

void camera_input_handle_key(Camera* cam, int key, int action)
{
	bool pressed = (action != GLFW_RELEASE);

	if (key == GLFW_KEY_W) {
		cam->move_forward = pressed;
	}
	if (key == GLFW_KEY_S) {
		cam->move_backward = pressed;
	}
	if (key == GLFW_KEY_A) {
		cam->move_left = pressed;
	}
	if (key == GLFW_KEY_D) {
		cam->move_right = pressed;
	}
	if (key == GLFW_KEY_Q) {
		cam->move_up = pressed;
	}
	if (key == GLFW_KEY_E) {
		cam->move_down = pressed;
	}
}

void camera_input_handle_mouse(Camera* cam, double xpos, double ypos)
{
	if (cam->first_mouse) {
		cam->last_mouse_x = xpos;
		cam->last_mouse_y = ypos;
		cam->first_mouse = false;
		return;
	}

	double delta_x = xpos - cam->last_mouse_x;
	double delta_y = ypos - cam->last_mouse_y;

	cam->last_mouse_x = xpos;
	cam->last_mouse_y = ypos;

	camera_process_mouse(cam, (float)delta_x, (float)delta_y);
}

void camera_input_handle_scroll(Camera* cam, double yoffset)
{
	camera_process_scroll(cam, (float)yoffset);
}

void camera_apply_gamepad(Camera* cam, const GamepadState* state)
{
	GamepadContext gctx = {
	    .yaw_target = cam->yaw_target,
	    .pitch_target = cam->pitch_target,
	    .fixed_timestep = cam->fixed_timestep,
	    .max_pitch = DEFAULT_MAX_PITCH,
	    .min_pitch = DEFAULT_MIN_PITCH,
	};
	glm_vec3_copy(cam->move_input, gctx.move_input);
	gamepad_write_input(state, &gctx);
	glm_vec3_copy(gctx.move_input, cam->move_input);
	cam->yaw_target = gctx.yaw_target;
	cam->pitch_target = gctx.pitch_target;
}
