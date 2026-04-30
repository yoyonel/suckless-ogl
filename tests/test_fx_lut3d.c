#include <glad/glad.h>

#include "effects/fx_lut3d.h"
#include "gpu_profiler.h"
#include "postprocess_internal.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>

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

static void test_lut3d_load_invalid_file(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &g_gpu_profiler_system, TEST_WIDTH,
	                 TEST_HEIGHT);
	int result = fx_lut3d_load_cube(&post_proc.lut3d_fx, &post_proc.lut3d,
	                                "non_existent.cube");
	TEST_ASSERT_NOT_EQUAL(0, result);
	postprocess_cleanup(&post_proc);
}

static void test_lut3d_load_valid_mock_cube(void)
{
	const char* lut_path = "tests/fixtures/test_lut.cube";
	FILE* file_ptr = fopen(lut_path, "w");
	if (!file_ptr) {
		TEST_FAIL_MESSAGE("Failed to create mock LUT file");
	}

	(void)fprintf(file_ptr, "TITLE \"Test LUT\"\n");
	(void)fprintf(file_ptr, "LUT_3D_SIZE 2\n");
	(void)fprintf(file_ptr, "0.0 0.0 0.0\n");
	(void)fprintf(file_ptr, "1.0 0.0 0.0\n");
	(void)fprintf(file_ptr, "0.0 1.0 0.0\n");
	(void)fprintf(file_ptr, "1.0 1.0 0.0\n");
	(void)fprintf(file_ptr, "0.0 0.0 1.0\n");
	(void)fprintf(file_ptr, "1.0 0.0 1.0\n");
	(void)fprintf(file_ptr, "0.0 1.0 1.0\n");
	(void)fprintf(file_ptr, "1.0 1.0 1.0\n");
	(void)fclose(file_ptr);

	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &g_gpu_profiler_system, TEST_WIDTH,
	                 TEST_HEIGHT);

	int result =
	    fx_lut3d_load_cube(&post_proc.lut3d_fx, &post_proc.lut3d, lut_path);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.lut3d.texture);

	/* Verify texture type */
	glBindTexture(GL_TEXTURE_3D, post_proc.lut3d.texture);
	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	postprocess_cleanup(&post_proc);
	(void)remove(lut_path);
}

static void test_lut3d_cleanup_removes_texture(void)
{
	const char* lut_path = "tests/fixtures/test_cleanup.cube";
	FILE* file_ptr = fopen(lut_path, "w");
	if (!file_ptr) {
		TEST_FAIL_MESSAGE("Failed to create mock LUT file");
	}
	(void)fprintf(file_ptr, "LUT_3D_SIZE 2\n");
	const int lut_entries = 8;
	for (int i = 0; i < lut_entries; i++) {
		(void)fprintf(file_ptr, "0.5 0.5 0.5\n");
	}
	(void)fclose(file_ptr);

	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &g_gpu_profiler_system, TEST_WIDTH,
	                 TEST_HEIGHT);
	(void)fx_lut3d_load_cube(&post_proc.lut3d_fx, &post_proc.lut3d,
	                         lut_path);
	GLuint tex = post_proc.lut3d.texture;
	TEST_ASSERT_TRUE(glIsTexture(tex));

	postprocess_cleanup(&post_proc);
	TEST_ASSERT_FALSE(glIsTexture(tex));

	(void)remove(lut_path);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_lut3d_load_invalid_file);
	RUN_TEST(test_lut3d_load_valid_mock_cube);
	RUN_TEST(test_lut3d_cleanup_removes_texture);
	return UNITY_END();
}
