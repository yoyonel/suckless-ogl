// tests/test_camera.c
#include "camera.h"
#include "unity.h"

void setUp(void)
{
}
void tearDown(void)
{
}

void test_camera_module_exists(void)
{
	Camera cam;
	camera_init(&cam, 20.0f, -90.0f, 0.0f);
	TEST_PASS();
}

void test_camera_initialization(void)
{
	Camera cam;
	camera_init(&cam, 20.0f, -90.0f, 0.0f);

	// Vérifier les angles (yaw et pitch sont bien initialisés)
	TEST_ASSERT_EQUAL_FLOAT(-90.0f, cam.yaw);
	TEST_ASSERT_EQUAL_FLOAT(0.0f, cam.pitch);
}

void test_camera_update_vectors(void)
{
	Camera cam;
	camera_init(&cam, 20.0f, -90.0f, 0.0f);
	camera_update_vectors(&cam);

	// Vérifier que les vecteurs sont normalisés (longueur ~1.0)
	float front_len =
	    sqrtf(cam.front[0] * cam.front[0] + cam.front[1] * cam.front[1] +
	          cam.front[2] * cam.front[2]);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, front_len);
}

void test_camera_process_keyboard(void)
{
	Camera cam;
	camera_init(&cam, 20.0f, -90.0f, 0.0f);
	cam.move_forward = 1;
	camera_process_keyboard(&cam, 0.016f);
	TEST_PASS();
}

void test_camera_process_mouse(void)
{
	Camera cam;
	camera_init(&cam, 20.0f, -90.0f, 0.0f);
	float initial_yaw = cam.yaw;
	camera_process_mouse(&cam, 10.0f, 5.0f);
	// Le yaw devrait avoir changé avec le mouvement de souris
	TEST_ASSERT_NOT_EQUAL(initial_yaw, cam.yaw);
}

void test_camera_get_view_matrix(void)
{
	Camera cam;
	mat4 view;
	camera_init(&cam, 20.0f, -90.0f, 0.0f);
	camera_get_view_matrix(&cam, view);
	TEST_PASS();
}

void test_camera_process_scroll(void)
{
	Camera cam;
	camera_init(&cam, 20.0f, -90.0f, 0.0f);
	camera_process_scroll(&cam, 1.0f);
	// Le scroll a été traité
	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_camera_module_exists);
	RUN_TEST(test_camera_initialization);
	RUN_TEST(test_camera_update_vectors);
	RUN_TEST(test_camera_process_keyboard);
	RUN_TEST(test_camera_process_mouse);
	RUN_TEST(test_camera_get_view_matrix);
	RUN_TEST(test_camera_process_scroll);
	return UNITY_END();
}