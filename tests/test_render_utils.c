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
static const int TEST_BUF_SIZE = 128;
static const int SMALL_BUF_SIZE = 10;

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

void test_render_utils_generate_gpu_identifier_basic(void)
{
	char buffer[TEST_BUF_SIZE];
	render_utils_generate_gpu_identifier("Intel", "Iris Xe", buffer,
	                                     sizeof(buffer));
	TEST_ASSERT_EQUAL_STRING("intel_iris_xe", buffer);
}

void test_render_utils_generate_gpu_identifier_messy(void)
{
	char buffer[TEST_BUF_SIZE];
	render_utils_generate_gpu_identifier("  Intel...Group  ",
	                                     "Mesa-123_456...GPU!!!", buffer,
	                                     sizeof(buffer));
	TEST_ASSERT_EQUAL_STRING("intel_group_mesa_123_456_gpu", buffer);
}

void test_render_utils_generate_gpu_identifier_truncation(void)
{
	char buffer[SMALL_BUF_SIZE];
	render_utils_generate_gpu_identifier("VeryLongVendorName",
	                                     "EvenLongerRendererName", buffer,
	                                     sizeof(buffer));
	TEST_ASSERT_EQUAL_STRING("verylongv", buffer);
}

void test_render_utils_generate_gpu_identifier_null_empty(void)
{
	char buffer[TEST_BUF_SIZE];
	render_utils_generate_gpu_identifier(NULL, NULL, buffer,
	                                     sizeof(buffer));
	TEST_ASSERT_EQUAL_STRING("unknown_gpu", buffer);

	render_utils_generate_gpu_identifier("", "", buffer, sizeof(buffer));
	TEST_ASSERT_EQUAL_STRING("unknown_gpu", buffer);
}

void test_render_utils_generate_gpu_identifier_separators(void)
{
	char buffer[TEST_BUF_SIZE];
	// Test consecutive separators collapse and trailing underscores are
	// trimmed
	render_utils_generate_gpu_identifier("A___B", "C...D---E ", buffer,
	                                     sizeof(buffer));
	TEST_ASSERT_EQUAL_STRING("a_b_c_d_e", buffer);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_render_utils_get_gpu_info);
	RUN_TEST(test_render_utils_generate_gpu_identifier_basic);
	RUN_TEST(test_render_utils_generate_gpu_identifier_messy);
	RUN_TEST(test_render_utils_generate_gpu_identifier_truncation);
	RUN_TEST(test_render_utils_generate_gpu_identifier_null_empty);
	RUN_TEST(test_render_utils_generate_gpu_identifier_separators);
	return UNITY_END();
}
