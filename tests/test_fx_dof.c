#include <glad/glad.h>

#include "gpu_profiler.h"
#include "postprocess_internal.h"
#include "pp_ubo.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>

static GLFWwindow* g_test_window = NULL;
static GPUProfiler g_gpu_profiler_system;

static const int TEST_WIDTH = 640;
static const int TEST_HEIGHT = 480;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	const int gl_major = 3;
	const int gl_minor = 3;

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl_major);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl_minor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	g_test_window = glfwCreateWindow(TEST_WIDTH, TEST_HEIGHT, "Test Window",
	                                 NULL, NULL);
	if (!g_test_window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create GLFW window");
	}

	glfwMakeContextCurrent(g_test_window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(g_test_window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to initialize GLAD");
	}

	gpu_profiler_init(&g_gpu_profiler_system);
}

void tearDown(void)
{
	gpu_profiler_cleanup(&g_gpu_profiler_system);
	if (g_test_window) {
		glfwDestroyWindow(g_test_window);
	}
	glfwTerminate();
}

static void test_dof_anamorphic_ratio_default(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &g_gpu_profiler_system, TEST_WIDTH,
	                 TEST_HEIGHT);

	/* Default should be 1.0 (spherical) */
	const float expected_ratio = 1.0F;
	const float epsilon = 1e-5F;
	TEST_ASSERT_FLOAT_WITHIN(epsilon, expected_ratio,
	                         post_proc.dof.anamorphic_ratio);

	postprocess_cleanup(&post_proc);
}

static void test_dof_anamorphic_ratio_setting(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &g_gpu_profiler_system, TEST_WIDTH,
	                 TEST_HEIGHT);

	const float test_ratio = 2.0F;
	postprocess_set_dof_anamorphic(&post_proc, test_ratio);

	const float epsilon = 1e-5F;
	TEST_ASSERT_FLOAT_WITHIN(epsilon, test_ratio,
	                         post_proc.dof.anamorphic_ratio);
	TEST_ASSERT_TRUE(post_proc.ubo_dirty);

	postprocess_cleanup(&post_proc);
}

static void test_ubo_packing_alignment(void)
{
	/* Verify that PostProcessUBO size is multiple of 16 (STD140 alignment)
	 */
	const size_t alignment = 16;
	TEST_ASSERT_EQUAL(0, sizeof(PostProcessUBO) % alignment);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_dof_anamorphic_ratio_default);
	RUN_TEST(test_dof_anamorphic_ratio_setting);
	RUN_TEST(test_ubo_packing_alignment);
	return UNITY_END();
}
