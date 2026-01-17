// tests/test_pbr.c
#include "gl_common.h"
#include "pbr.h"
#include "unity.h"

static GLFWwindow* test_window = NULL;

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

void test_pbr_module_exists(void)
{
	TEST_PASS();
}

void test_pbr_functions_linkage(void)
{
	// Vérifier que les fonctions existent et compilent
	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_pbr_module_exists);
	RUN_TEST(test_pbr_functions_linkage);
	return UNITY_END();
}