#include <glad/glad.h>

#include "perf_timer.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>

static GLFWwindow* test_window = NULL;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	test_window = glfwCreateWindow(640, 480, "Test Window", NULL, NULL);
	if (!test_window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create GLFW window");
	}

	glfwMakeContextCurrent(test_window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(test_window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to initialize GLAD");
	}
}

void tearDown(void)
{
	if (test_window) {
		glfwDestroyWindow(test_window);
	}
	glfwTerminate();
}

void test_gpu_timer_is_non_blocking(void)
{
	GPUTimer gpu_timer = {0};
	PerfTimer cpu_timer = {0};

	// 1. Start GPU timer
	gpu_timer_start(&gpu_timer);

	// 2. Issue heavy GPU work
	// Clearing the screen many times to simulate load
	// This is not super heavy but enough to cause measurable delay if
	// glFinish waits for it compared to almost instant return.
	for (int i = 0; i < 1000; i++) {
		glClearColor((i % 255) / 255.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	// We do NOT call glFinish here explicitly. We want to see if
	// gpu_timer_elapsed_ms calls it.

	// 3. Measure time taken by gpu_timer_elapsed_ms with wait_for_result=0
	perf_timer_start(&cpu_timer);

	// Calling with wait_for_result=0.
	// If glFinish is present, this will block until all 1000 clears are
	// done. If glFinish is removed, this should return almost immediately
	// (issuing query and checking status).
	double result = gpu_timer_elapsed_ms(&gpu_timer, 0);

	double cpu_elapsed_ms = perf_timer_elapsed_ms(&cpu_timer);

	printf("CPU time for gpu_timer_elapsed_ms(0): %f ms\n", cpu_elapsed_ms);

	// Clean up
	gpu_timer_cleanup(&gpu_timer);

	// Assertion:
	// If the optimization is working, this call should be very fast (e.g.,
	// < 0.5ms). If glFinish is present, it might take > 1ms depending on
	// GPU speed, but 1000 clears should take some time. Let's be
	// conservative and say if it takes > 2ms it's likely blocking. Ideally
	// we'd compare against a baseline, but "non-blocking" means "fast".
	// 0.5ms is generous for just issuing a query and checking a bool.
	TEST_ASSERT_LESS_THAN_FLOAT(0.5f, (float)cpu_elapsed_ms);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gpu_timer_is_non_blocking);
	return UNITY_END();
}
