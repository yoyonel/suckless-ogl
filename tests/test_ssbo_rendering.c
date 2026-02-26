// tests/test_ssbo_rendering.c
#include "gl_common.h"
#include "ssbo_rendering.h"
#include "unity.h"
#include "utils.h"
#include <string.h>

static GLFWwindow* test_window = NULL;

static const int INSTANCE_COUNT = 2;

void setUp(void)
{
	if (!glfwInit()) {
		return;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	test_window = glfwCreateWindow(1, 1, "Test", NULL, NULL);
	if (!test_window) {
		glfwTerminate();
		return;
	}

	glfwMakeContextCurrent(test_window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
}

void tearDown(void)
{
	if (test_window) {
		glfwDestroyWindow(test_window);
		test_window = NULL;
	}
	glfwTerminate();
}

void test_ssbo_rendering_module_exists(void)
{
	TEST_PASS();
}

void test_ssbo_rendering_init_cleanup(void)
{
	if (!test_window) {
		TEST_IGNORE_MESSAGE("OpenGL context not available");
	}

	SSBOGroup group;
	SphereInstanceSSBO instances[INSTANCE_COUNT];
	(void)safe_memset(instances, sizeof(instances), 0, sizeof(instances));

	ssbo_group_init(&group, instances, INSTANCE_COUNT);
	ssbo_group_cleanup(&group);

	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_ssbo_rendering_module_exists);
	RUN_TEST(test_ssbo_rendering_init_cleanup);
	return UNITY_END();
}
