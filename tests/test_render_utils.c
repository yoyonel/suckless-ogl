// tests/test_render_utils.c
#include "render_utils.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <string.h>

static GLFWwindow* window = NULL;

static const int WIN_WIDTH = 640;
static const int WIN_HEIGHT = 480;
static const int GL_VER_MAJOR = 3;
static const int GL_VER_MINOR = 3;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_VER_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_VER_MINOR);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window =
	    glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Test Window", NULL, NULL);
	if (!window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create GLFW window");
	}

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to initialize GLAD");
	}
}

void tearDown(void)
{
	if (window) {
		glfwDestroyWindow(window);
	}
	glfwTerminate();
}

void test_render_utils_get_gpu_info(void)
{
	GPUInfo info = render_utils_get_gpu_info();
	TEST_ASSERT_NOT_NULL(info.vendor);
	TEST_ASSERT_NOT_NULL(info.renderer);
	TEST_ASSERT_NOT_NULL(info.version);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_render_utils_get_gpu_info);
	return UNITY_END();
}
