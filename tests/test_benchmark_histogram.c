// tests/test_benchmark_histogram.c
#include "app.h"
#include "app_ui.h"
#include "glad/glad.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GLFWwindow* test_window = NULL;

void setUp(void)
{
	if (!glfwInit()) {
		printf("Failed to init GLFW\n");
		return;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	test_window = glfwCreateWindow(1, 1, "Test", NULL, NULL);
	if (!test_window) {
		printf("Failed to create window\n");
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

void test_benchmark_histogram(void)
{
	if (!test_window) {
		TEST_IGNORE_MESSAGE("OpenGL context not available");
	}

	// Setup Dummy Texture
	const int MAP_SIZE = 64;
	const int TOTAL_PIXELS = MAP_SIZE * MAP_SIZE;
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, MAP_SIZE, MAP_SIZE, 0, GL_RED,
	             GL_FLOAT, NULL);

	// Fill with some data
	float* data = malloc(TOTAL_PIXELS * sizeof(float));
	for (int i = 0; i < TOTAL_PIXELS; i++) {
		data[i] = (float)(i % 100) / 100.0f;  // 0.0 to 0.99
	}
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MAP_SIZE, MAP_SIZE, GL_RED,
	                GL_FLOAT, data);
	free(data);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Setup App
	App app = {0};
	app.postprocess.auto_exposure_fx.downsample_tex = tex;

	// Initialize PBO for the test
	glGenBuffers(1, &app.histogram_pbo);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, app.histogram_pbo);
	glBufferData(GL_PIXEL_PACK_BUFFER, 64 * 64 * sizeof(float), NULL,
	             GL_STREAM_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	// Output buffers
	const int HISTO_SIZE = 64;
	int buckets[HISTO_SIZE];
	float min_lum = 0.0f;
	float max_lum = 0.0f;

	// Warmup
	compute_luminance_histogram(&app, buckets, HISTO_SIZE, &min_lum,
	                            &max_lum);

	// Benchmark
	clock_t start = clock();
	int iterations = 1000;
	for (int i = 0; i < iterations; i++) {
		compute_luminance_histogram(&app, buckets, HISTO_SIZE, &min_lum,
		                            &max_lum);
	}
	clock_t end = clock();
	double cpu_time_used =
	    ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;  // ms

	printf("Benchmark Result: %d iterations took %.2f ms (%.4f ms/call)\n",
	       iterations, cpu_time_used, cpu_time_used / iterations);

	// Verify
	int total_buckets = 0;
	for (int i = 0; i < HISTO_SIZE; i++) {
		total_buckets += buckets[i];
	}
	TEST_ASSERT_GREATER_THAN(0, total_buckets);

	// Cleanup
	glDeleteTextures(1, &tex);
	glDeleteBuffers(1, &app.histogram_pbo);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_benchmark_histogram);
	return UNITY_END();
}
