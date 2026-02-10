#define _POSIX_C_SOURCE 199309L
#include "app.h"
#include "app_scene.h"
#include "gl_common.h"
#include "main.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <time.h>

static App g_test_app;
static bool g_app_initialized = false;

static const float TEST_CAMERA_Z = 50.0F;
static const float TEST_CAMERA_YAW = -90.0F;
static const float TEST_CAMERA_PITCH = 0.0F;
static const int POLL_TIMEOUT = 100;

void setUp(void)
{
	if (!g_app_initialized) {
		int result = app_init(&g_test_app, WINDOW_WIDTH, WINDOW_HEIGHT,
		                      "Stencil Test");
		TEST_ASSERT_EQUAL_INT(1, result);
		g_app_initialized = true;
	}
}

void tearDown(void)
{
}

void test_stencil_masking_values(void)
{
	/* 1. Ensure geometry is loaded */
	icosphere_generate(&g_test_app.geometry, 3);
	app_update_gpu_buffers(&g_test_app);

	/* 2. Set camera to look at center area */
	g_test_app.camera.position[0] = 0.0F;
	g_test_app.camera.position[1] = 0.0F;
	g_test_app.camera.position[2] = TEST_CAMERA_Z;
	g_test_app.camera.yaw = TEST_CAMERA_YAW;
	g_test_app.camera.pitch = TEST_CAMERA_PITCH;
	camera_update_vectors(&g_test_app.camera);

	/* 3. Wait for scene to be ready */
	for (int i = 0; i < POLL_TIMEOUT; i++) {
		app_update(&g_test_app);
		glfwPollEvents();
	}

	/* 4. Render a frame */
	app_render(&g_test_app);

	/* 5. Bind the scene FBO to read from it */
	glBindFramebuffer(GL_FRAMEBUFFER, g_test_app.postprocess.scene_fbo);

	/* 6. Check if any pixel in the stencil buffer has value 1 */
	unsigned char* stencil_buf =
	    malloc((size_t)(WINDOW_WIDTH * WINDOW_HEIGHT));
	TEST_ASSERT_NOT_NULL(stencil_buf);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_STENCIL_INDEX,
	             GL_UNSIGNED_BYTE, stencil_buf);

	int found_stencil_1 = 0;
	for (int i = 0; i < WINDOW_WIDTH * WINDOW_HEIGHT; i++) {
		if (stencil_buf[i] == 1) {
			found_stencil_1 = 1;
			break;
		}
	}

	/* 7. Check corner pixel (should be skybox) */
	unsigned char corner_stencil = stencil_buf[0];

	free(stencil_buf);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	TEST_ASSERT_TRUE_MESSAGE(
	    found_stencil_1, "At least one pixel should have stencil value 1");
	TEST_ASSERT_EQUAL_UINT8_MESSAGE(
	    0, corner_stencil,
	    "Corner pixel (skybox) should have stencil value 0");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_stencil_masking_values);

	if (g_app_initialized) {
		app_cleanup(&g_test_app);
	}

	return UNITY_END();
}
