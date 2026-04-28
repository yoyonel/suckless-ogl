#define _POSIX_C_SOURCE 200809L
#include "app.h"
#include "gl_common.h"
#include "main.h"
#include "renderer.h"
#include "scene.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

/* Use a downsampled resolution for faster, less flaky tests */
enum {
	TEST_WIDTH = WINDOW_WIDTH / 8,   /* 128 */
	TEST_HEIGHT = WINDOW_HEIGHT / 8, /* 96  */
};

static App g_test_app;
static bool g_app_initialized = false;

static void test_render_ui_trampoline(void* user_data)
{
	app_render_ui((const App*)user_data);
}

static RenderContext test_render_ctx_from_app(App* app)
{
	return (RenderContext){
	    .scene = &app->scene,
	    .postprocess = &app->postprocess,
	    .camera = &app->input.camera,
	    .profiler = &app->profiling.gpu_profiler,
	    .profiler_ui = &app->profiling.timeline_ui,
	    .env_mgr = &app->env_mgr,
	    .notifier = &app->notifier,
	    .effect_bench = &app->effect_bench,
	    .width = app->width,
	    .height = app->height,
	    .delta_time = app->delta_time,
	    .frame_count = app->frame_count,
	    .log_gpu_metrics = app->profiling.log_gpu_metrics,
	    .render_ui = test_render_ui_trampoline,
	    .render_ui_data = app,
	};
}

static const float TEST_CAMERA_Z = 50.0F;
static const float TEST_CAMERA_YAW = -90.0F;
static const float TEST_CAMERA_PITCH = 0.0F;
static const int POLL_TIMEOUT = 100;
static const float DEPTH_SKYBOX_THRESHOLD = 1e-6F;
static const int MAX_MISMATCH_LOG = 5;

void setUp(void)
{
	if (!g_app_initialized) {
		int result = app_init(&g_test_app, TEST_WIDTH, TEST_HEIGHT,
		                      "Stencil Test");
		TEST_ASSERT_EQUAL_INT(1, result);
		g_app_initialized = true;
	}
}

void tearDown(void)
{
}

/**
 * Cross-validate depth and stencil buffers:
 *   depth == 1.0  (skybox)  → stencil must be 0
 *   depth <  1.0  (object)  → stencil must be 1
 */
void test_stencil_depth_consistency(void)
{
	/* 1. Ensure scene is ready */

	/* 2. Set camera to look at center area */
	g_test_app.input.camera.position[0] = 0.0F;
	g_test_app.input.camera.position[1] = 0.0F;
	g_test_app.input.camera.position[2] = TEST_CAMERA_Z;
	g_test_app.input.camera.yaw = TEST_CAMERA_YAW;
	g_test_app.input.camera.pitch = TEST_CAMERA_PITCH;
	camera_update_vectors(&g_test_app.input.camera);

	/* 3. Wait for scene to be ready */
	for (int i = 0; i < POLL_TIMEOUT; i++) {
		app_update(&g_test_app);
		glfwPollEvents();
	}

	/* 4. Render a frame */
	{
		RenderContext rctx = test_render_ctx_from_app(&g_test_app);
		renderer_draw_frame(&rctx);
	}

	/* 5. Bind the scene FBO to read from it */
	glBindFramebuffer(GL_FRAMEBUFFER, g_test_app.postprocess.scene_fbo);

	size_t pixel_count = (size_t)(TEST_WIDTH * TEST_HEIGHT);

	/* 6. Read stencil buffer */
	unsigned char* stencil_buf = malloc(pixel_count);
	TEST_ASSERT_NOT_NULL(stencil_buf);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, TEST_WIDTH, TEST_HEIGHT, GL_STENCIL_INDEX,
	             GL_UNSIGNED_BYTE, stencil_buf);

	/* 7. Read depth buffer */
	float* depth_buf = malloc(pixel_count * sizeof(float));
	TEST_ASSERT_NOT_NULL(depth_buf);

	glReadPixels(0, 0, TEST_WIDTH, TEST_HEIGHT, GL_DEPTH_COMPONENT,
	             GL_FLOAT, depth_buf);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	/* 8. Cross-validate every pixel */
	int skybox_count = 0;
	int object_count = 0;
	int mismatches = 0;

	for (size_t i = 0; i < pixel_count; i++) {
		bool is_skybox =
		    (fabsf(depth_buf[i] - 1.0F) < DEPTH_SKYBOX_THRESHOLD);
		unsigned char expected_stencil =
		    is_skybox ? (unsigned char)0 : (unsigned char)1;

		if (stencil_buf[i] != expected_stencil) {
			if (mismatches < MAX_MISMATCH_LOG) {
				int col = (int)(i % (size_t)TEST_WIDTH);
				int row = (int)(i / (size_t)TEST_WIDTH);
				printf(
				    "  MISMATCH at (%d,%d): depth=%.6f "
				    "stencil=%u expected=%u\n",
				    col, row, (double)depth_buf[i],
				    stencil_buf[i], expected_stencil);
			}
			mismatches++;
		}

		if (is_skybox) {
			skybox_count++;
		} else {
			object_count++;
		}
	}

	free(depth_buf);
	free(stencil_buf);

	/* 9. Report statistics */
	printf("  Resolution: %dx%d (%zu pixels)\n", TEST_WIDTH, TEST_HEIGHT,
	       pixel_count);
	printf("  Skybox pixels: %d  |  Object pixels: %d\n", skybox_count,
	       object_count);
	printf("  Mismatches: %d\n", mismatches);

	/* 10. Assertions */
	TEST_ASSERT_GREATER_THAN_INT_MESSAGE(
	    0, object_count,
	    "Scene must contain at least one object pixel (depth < 1.0)");
	TEST_ASSERT_GREATER_THAN_INT_MESSAGE(
	    0, skybox_count,
	    "Scene must contain at least one skybox pixel (depth == 1.0)");
	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    0, mismatches,
	    "Every pixel must satisfy: depth==1.0 → stencil==0, "
	    "depth<1.0 → stencil==1");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_stencil_depth_consistency);

	if (g_app_initialized) {
		app_cleanup(&g_test_app);
	}

	return UNITY_END();
}
