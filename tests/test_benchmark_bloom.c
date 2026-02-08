#include <glad/glad.h>

#include "gpu_profiler.h"
#include "postprocess.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <time.h>

static GLFWwindow* test_window = NULL;
static GPUProfiler gpu_profiler_system;
static const int TestWidth = 1920;
static const int TestHeight = 1080;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	test_window =
	    glfwCreateWindow(TestWidth, TestHeight, "Test Window", NULL, NULL);
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
	gpu_profiler_init(&gpu_profiler_system);
}

void tearDown(void)
{
	gpu_profiler_cleanup(&gpu_profiler_system);
	if (test_window)
		glfwDestroyWindow(test_window);
	glfwTerminate();
}

void test_benchmark_bloom_render(void)
{
	PostProcess post_proc = {0};
	int result = postprocess_init(&post_proc, &gpu_profiler_system,
	                              TestWidth, TestHeight);
	TEST_ASSERT_EQUAL(1, result);

	// Enable bloom
	postprocess_enable(&post_proc, POSTFX_BLOOM);
	post_proc.bloom.intensity = 1.0f;
	post_proc.bloom.threshold = 0.5f;

	// Warmup
	fx_bloom_render(&post_proc);

	// Benchmark
	clock_t start = clock();
	int iterations = 100000;
	for (int i = 0; i < iterations; ++i) {
		fx_bloom_render(&post_proc);
	}
	clock_t end = clock();
	double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

	printf("Bloom Render Time for %d iterations: %f seconds\n", iterations,
	       cpu_time_used);

	postprocess_cleanup(&post_proc);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_benchmark_bloom_render);
	return UNITY_END();
}
