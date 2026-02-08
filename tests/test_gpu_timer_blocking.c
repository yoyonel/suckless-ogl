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

void test_gpu_timer_is_non_blocking_benchmark(void)
{
	GPUTimer gpu_timer = {0};
	PerfTimer cpu_timer = {0};
	const int ITERATIONS = 100;
	double total_ms = 0.0;
	double min_ms = 1e9;
	double max_ms = 0.0;

	printf(
	    "Benchmarking gpu_timer_elapsed_ms(wait=0) over %d iterations...\n",
	    ITERATIONS);

	for (int iter = 0; iter < ITERATIONS; ++iter) {
		// 1. Start GPU timer
		gpu_timer_start(&gpu_timer);

		// 2. Issue heavy GPU work
		for (int i = 0; i < 1000; i++) {
			glClearColor((i % 255) / 255.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		// 3. Measure time taken by gpu_timer_elapsed_ms with
		// wait_for_result=0
		perf_timer_start(&cpu_timer);

		// This call should return almost immediately
		double result = gpu_timer_elapsed_ms(&gpu_timer, 0);
		(void)result;

		double elapsed = perf_timer_elapsed_ms(&cpu_timer);

		total_ms += elapsed;
		if (elapsed < min_ms)
			min_ms = elapsed;
		if (elapsed > max_ms)
			max_ms = elapsed;

		// Clean up for next iteration
		gpu_timer_cleanup(&gpu_timer);
	}

	double avg_ms = total_ms / ITERATIONS;
	printf("Results:\n");
	printf("  Min: %.4f ms\n", min_ms);
	printf("  Max: %.4f ms\n", max_ms);
	printf("  Avg: %.4f ms\n", avg_ms);

	// Assertion: Average should be very fast (< 0.5ms)
	TEST_ASSERT_LESS_THAN_FLOAT(0.5f, (float)avg_ms);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gpu_timer_is_non_blocking_benchmark);
	return UNITY_END();
}
