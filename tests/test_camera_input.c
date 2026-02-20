#include "camera.h"
#include "camera_input.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <math.h>

void setUp(void)
{
}
void tearDown(void)
{
}

void test_camera_input_handle_key_should_update_movement_flags(void)
{
	Camera cam;
	camera_init(&cam, 10.0F, 0.0F, 0.0F);

	/* Initial state */
	TEST_ASSERT_EQUAL(0, cam.move_forward);

	/* Press W */
	camera_input_handle_key(&cam, GLFW_KEY_W, GLFW_PRESS);
	TEST_ASSERT_EQUAL(1, cam.move_forward);

	/* Release W */
	camera_input_handle_key(&cam, GLFW_KEY_W, GLFW_RELEASE);
	TEST_ASSERT_EQUAL(0, cam.move_forward);
}

void test_camera_input_handle_mouse_should_handle_first_mouse(void)
{
	Camera cam;
	camera_init(&cam, 10.0F, 90.0F, 0.0F);

	/* Ensure initialized state */
	TEST_ASSERT_EQUAL(1, cam.first_mouse);
	TEST_ASSERT_EQUAL_FLOAT(0.0F, cam.last_mouse_x);
	TEST_ASSERT_EQUAL_FLOAT(0.0F, cam.last_mouse_y);

	/* First mouse event */
	camera_input_handle_mouse(&cam, 100.0, 200.0);

	/* Verify state updated but no movement applied (as deltas are 0
	 * effectively) */
	TEST_ASSERT_EQUAL(0, cam.first_mouse);
	TEST_ASSERT_EQUAL_FLOAT(100.0F, cam.last_mouse_x);
	TEST_ASSERT_EQUAL_FLOAT(200.0F, cam.last_mouse_y);

	/* Verify logic: first mouse event just sets the position, does not call
	 * process_mouse with huge deltas */
	/* If it called process_mouse with 100, 200, yaw/pitch would change */
	/* Here we expect NO change because logic returns early */
	TEST_ASSERT_EQUAL_FLOAT(90.0F, cam.yaw);
	TEST_ASSERT_EQUAL_FLOAT(0.0F, cam.pitch);
}

void test_camera_input_handle_mouse_should_update_orientation(void)
{
	Camera cam;
	camera_init(&cam, 10.0F, 90.0F, 0.0F);
	cam.first_mouse = 0;
	cam.last_mouse_x = 100.0;
	cam.last_mouse_y = 100.0;

	/* Move mouse right (positive X) */
	camera_input_handle_mouse(&cam, 110.0, 100.0);

	/* Check state update */
	TEST_ASSERT_EQUAL_FLOAT(110.0F, cam.last_mouse_x);

	/* Check orientation update */
	/* Expected: Yaw increases (looking right) */
	/* Actual value depends on smoothing and sensitivity, but delta > 0 */
	TEST_ASSERT_TRUE(cam.yaw_target > 90.0F);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_camera_input_handle_key_should_update_movement_flags);
	RUN_TEST(test_camera_input_handle_mouse_should_handle_first_mouse);
	RUN_TEST(test_camera_input_handle_mouse_should_update_orientation);
	return UNITY_END();
}
