#include "camera.h"
#include "gamepad_input.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

/* ---- Stub for log_message (avoids linking log.c + platform deps) ---- */
void log_message(int level, const char* tag, const char* fmt, ...)
{
	(void)level;
	(void)tag;
	(void)fmt;
}

/* ---- GLFW gamepad mock infrastructure ---- */

static int g_mock_gamepad_connected = 0;
static GLFWgamepadstate g_mock_gamepad_state;

int glfwJoystickIsGamepad(int jid)
{
	(void)jid;
	return g_mock_gamepad_connected;
}

int glfwGetGamepadState(int jid, GLFWgamepadstate* state)
{
	(void)jid;
	if (!g_mock_gamepad_connected) {
		return GLFW_FALSE;
	}
	*state = g_mock_gamepad_state;
	return GLFW_TRUE;
}

const char* glfwGetGamepadName(int jid)
{
	(void)jid;
	return g_mock_gamepad_connected ? "Mock DualShock 4" : NULL;
}

static void mock_gamepad_reset(void)
{
	g_mock_gamepad_connected = 0;
	memset(&g_mock_gamepad_state, 0, sizeof(g_mock_gamepad_state));
	/* Triggers default to released = -1.0 in GLFW. */
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = -1.0F;
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = -1.0F;
}

void setUp(void)
{
	mock_gamepad_reset();
}
void tearDown(void)
{
}

/* ---- gamepad_input_init ---- */

void test_gamepad_init_sets_defaults(void)
{
	GamepadState state;
	gamepad_input_init(&state);

	TEST_ASSERT_EQUAL(0, state.connected);
	TEST_ASSERT_EQUAL(0, state.joystick_id);
	TEST_ASSERT_FLOAT_WITHIN(0.001F, GAMEPAD_DEFAULT_DEADZONE,
	                         state.deadzone);
	TEST_ASSERT_FLOAT_WITHIN(0.001F, GAMEPAD_DEFAULT_LOOK_SENSITIVITY,
	                         state.look_sensitivity);
	TEST_ASSERT_FLOAT_WITHIN(0.001F, GAMEPAD_DEFAULT_MOVE_SENSITIVITY,
	                         state.move_sensitivity);
	TEST_ASSERT_FLOAT_WITHIN(0.001F, GAMEPAD_DEFAULT_TRIGGER_THRESHOLD,
	                         state.trigger_threshold);
}

/* ---- gamepad_apply_deadzone ---- */

void test_deadzone_returns_zero_inside_threshold(void)
{
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F,
	                         gamepad_apply_deadzone(0.10F, 0.15F));
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F,
	                         gamepad_apply_deadzone(-0.10F, 0.15F));
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F,
	                         gamepad_apply_deadzone(0.0F, 0.15F));
	/* Exactly at deadzone boundary. */
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F,
	                         gamepad_apply_deadzone(0.149F, 0.15F));
}

void test_deadzone_rescales_outside_threshold(void)
{
	/* Full positive input: (1.0 - 0.15) / (1.0 - 0.15) = 1.0 */
	TEST_ASSERT_FLOAT_WITHIN(0.001F, 1.0F,
	                         gamepad_apply_deadzone(1.0F, 0.15F));
	/* Full negative input: -1.0 */
	TEST_ASSERT_FLOAT_WITHIN(0.001F, -1.0F,
	                         gamepad_apply_deadzone(-1.0F, 0.15F));
	/* Midpoint: (0.575 - 0.15) / (1.0 - 0.15) = 0.425 / 0.85 ≈ 0.5 */
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 0.5F,
	                         gamepad_apply_deadzone(0.575F, 0.15F));
	/* Small value just outside: (0.20 - 0.15) / (0.85) ≈ 0.059 */
	float result = gamepad_apply_deadzone(0.20F, 0.15F);
	TEST_ASSERT_TRUE(result > 0.0F);
	TEST_ASSERT_TRUE(result < 0.1F);
}

void test_deadzone_preserves_sign(void)
{
	float pos = gamepad_apply_deadzone(0.5F, 0.15F);
	float neg = gamepad_apply_deadzone(-0.5F, 0.15F);
	TEST_ASSERT_TRUE(pos > 0.0F);
	TEST_ASSERT_TRUE(neg < 0.0F);
	TEST_ASSERT_FLOAT_WITHIN(0.001F, fabsf(pos), fabsf(neg));
}

void test_deadzone_zero_threshold(void)
{
	/* With zero dead-zone, pass-through. */
	TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.5F,
	                         gamepad_apply_deadzone(0.5F, 0.0F));
	TEST_ASSERT_FLOAT_WITHIN(0.001F, -0.3F,
	                         gamepad_apply_deadzone(-0.3F, 0.0F));
}

/* ---- gamepad_input_poll (no gamepad connected → early return) ---- */

void test_poll_without_gamepad_does_nothing(void)
{
	GamepadState state;
	gamepad_input_init(&state);

	Camera cam;
	camera_init(&cam, 10.0F, 0.0F, 0.0F);

	float yaw_before = cam.yaw_target;

	gamepad_input_poll(&state, NULL);
	camera_build_keyboard_input(&cam);
	gamepad_write_input(&state, &cam);

	TEST_ASSERT_EQUAL(0, state.connected);
	/* move_input should stay zero. */
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, cam.move_input[0]);
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, cam.move_input[1]);
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, cam.move_input[2]);
	TEST_ASSERT_FLOAT_WITHIN(0.001F, yaw_before, cam.yaw_target);
}

/* ---- gamepad_input_poll (connected, sticks ≠ 0) ---- */

void test_poll_left_stick_forward_adds_velocity(void)
{
	GamepadState state;
	gamepad_input_init(&state);

	Camera cam;
	camera_init(&cam, 10.0F, 0.0F, 0.0F);
	glm_vec3_zero(cam.velocity_current);

	g_mock_gamepad_connected = 1;
	/* Stick up = ly negative in GLFW. */
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = -1.0F;

	gamepad_input_poll(&state, NULL);
	camera_build_keyboard_input(&cam);
	gamepad_write_input(&state, &cam);

	TEST_ASSERT_EQUAL(1, state.connected);
	/* move_input[2] should be positive (forward). */
	TEST_ASSERT_TRUE(cam.move_input[2] > 0.0F);

	/* After a physics step, velocity should be non-zero. */
	camera_fixed_update(&cam);
	float speed = glm_vec3_norm(cam.velocity_current);
	TEST_ASSERT_TRUE(speed > 0.0F);
}

void test_poll_right_stick_yaw(void)
{
	GamepadState state;
	gamepad_input_init(&state);

	Camera cam;
	camera_init(&cam, 10.0F, 90.0F, 0.0F);

	float yaw_before = cam.yaw_target;

	g_mock_gamepad_connected = 1;
	/* Push right stick to the right. */
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = 1.0F;

	gamepad_input_poll(&state, NULL);
	camera_build_keyboard_input(&cam);
	gamepad_write_input(&state, &cam);

	/* Yaw should have increased (looking right). */
	TEST_ASSERT_TRUE(cam.yaw_target > yaw_before);
}

void test_poll_triggers_vertical_movement(void)
{
	GamepadState state;
	gamepad_input_init(&state);

	Camera cam;
	camera_init(&cam, 10.0F, 0.0F, 0.0F);
	glm_vec3_zero(cam.velocity_current);

	g_mock_gamepad_connected = 1;
	/* R2 fully pressed (GLFW: -1 released, +1 pressed). */
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = 1.0F;

	gamepad_input_poll(&state, NULL);
	camera_build_keyboard_input(&cam);
	gamepad_write_input(&state, &cam);

	/* move_input[1] should be positive (up). */
	TEST_ASSERT_TRUE(cam.move_input[1] > 0.0F);
}

void test_poll_left_trigger_moves_down(void)
{
	GamepadState state;
	gamepad_input_init(&state);

	Camera cam;
	camera_init(&cam, 10.0F, 0.0F, 0.0F);
	glm_vec3_zero(cam.velocity_current);

	g_mock_gamepad_connected = 1;
	/* L2 fully pressed. */
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = 1.0F;

	gamepad_input_poll(&state, NULL);
	camera_build_keyboard_input(&cam);
	gamepad_write_input(&state, &cam);

	/* move_input[1] should be negative (down). */
	TEST_ASSERT_TRUE(cam.move_input[1] < 0.0F);
}

void test_poll_sticks_in_deadzone_no_effect(void)
{
	GamepadState state;
	gamepad_input_init(&state);

	Camera cam;
	camera_init(&cam, 10.0F, 90.0F, 0.0F);
	glm_vec3_zero(cam.velocity_current);
	float yaw_before = cam.yaw_target;

	g_mock_gamepad_connected = 1;
	/* All axes within deadzone. */
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] = 0.05F;
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = -0.03F;
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = 0.02F;
	g_mock_gamepad_state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = -0.01F;

	gamepad_input_poll(&state, NULL);
	camera_build_keyboard_input(&cam);
	gamepad_write_input(&state, &cam);

	/* All inputs should be zero (within deadzone). */
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, cam.move_input[0]);
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, cam.move_input[2]);
	TEST_ASSERT_FLOAT_WITHIN(0.0001F, yaw_before, cam.yaw_target);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gamepad_init_sets_defaults);
	RUN_TEST(test_deadzone_returns_zero_inside_threshold);
	RUN_TEST(test_deadzone_rescales_outside_threshold);
	RUN_TEST(test_deadzone_preserves_sign);
	RUN_TEST(test_deadzone_zero_threshold);
	RUN_TEST(test_poll_without_gamepad_does_nothing);
	RUN_TEST(test_poll_left_stick_forward_adds_velocity);
	RUN_TEST(test_poll_right_stick_yaw);
	RUN_TEST(test_poll_triggers_vertical_movement);
	RUN_TEST(test_poll_left_trigger_moves_down);
	RUN_TEST(test_poll_sticks_in_deadzone_no_effect);
	return UNITY_END();
}
