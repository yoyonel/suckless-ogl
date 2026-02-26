#include "gl_common.h"
#include "instanced_rendering.h"
#include "unity.h"
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

void test_instanced_rendering_module_exists(void)
{
	TEST_PASS();
}

void test_instanced_rendering_init_cleanup(void)
{
	if (!test_window) {
		TEST_IGNORE_MESSAGE("OpenGL context not available");
	}

	InstancedGroup group;
	SphereInstance instances[INSTANCE_COUNT];
	// clear memory
	(void)memset(instances, 0, sizeof(instances));

	instanced_group_init(&group, instances, INSTANCE_COUNT);
	instanced_group_cleanup(&group);

	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_instanced_rendering_module_exists);
	RUN_TEST(test_instanced_rendering_init_cleanup);
	return UNITY_END();
}
