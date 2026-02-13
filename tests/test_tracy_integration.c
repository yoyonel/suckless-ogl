#include "glad/glad.h"
#include "tracy_ogl_bridge.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>

void setUp(void)
{
}
void tearDown(void)
{
}

void test_tracy_init(void)
{
#ifdef TRACY_ENABLE
	// TracyOGL_Init requires a GL context.
	if (!glfwInit()) {
		TEST_IGNORE_MESSAGE("GLFW Init Failed");
		return;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	GLFWwindow* window =
	    glfwCreateWindow(640, 480, "Tracy Test", NULL, NULL);
	if (!window) {
		glfwTerminate();
		TEST_IGNORE_MESSAGE("Window Creation Failed");
		return;
	}

	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("GLAD Load Failed");
		return;
	}

	// Call Tracy Init
	TracyOGL_Init();

	// Simulate a zone
	TracySourceLocationData loc = {"TestZone", "test_tracy_init",
	                               "tests/test_tracy_integration.c",
	                               __LINE__, 0xFF0000};
	void* tracy_scope = TracyOGL_ZoneBegin(&loc);
	TracyOGL_ZoneEnd(tracy_scope);

	TracyOGL_Collect();

	// Cleanup
	TracyOGL_Destroy();

	glfwDestroyWindow(window);
	glfwTerminate();

	printf("Tracy Integration Test Passed (Enabled)\n");
#else
	printf("Tracy Integration Test Passed (Disabled)\n");
#endif
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_tracy_init);
	return UNITY_END();
}
