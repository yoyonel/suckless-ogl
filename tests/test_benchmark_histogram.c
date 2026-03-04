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

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static GLFWwindow* test_window = NULL;

static const int MAP_SIZE = 64;
static const int HISTO_SIZE = 64;
static const int ITERATIONS = 1000;
static const float DATA_DIVISOR = 100.0F;
static const double MS_MULTIPLIER = 1000.0;

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
	const int TOTAL_PIXELS = MAP_SIZE * MAP_SIZE;
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, MAP_SIZE, MAP_SIZE, 0, GL_RED,
	             GL_FLOAT, NULL);

	// Fill with some data
	float* data = (float*)malloc((size_t)TOTAL_PIXELS * sizeof(float));
	if (!data) {
		TEST_FAIL_MESSAGE("Failed to allocate memory for texture data");
		return;  // Redundant but safe
	}
	const int MODULO_VAL = 100;
	for (int i = 0; i < TOTAL_PIXELS; i++) {
		data[i] =
		    (float)(i % MODULO_VAL) / DATA_DIVISOR;  // 0.0 to 0.99
	}
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MAP_SIZE, MAP_SIZE, GL_RED,
	                GL_FLOAT, data);
	free(data);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Setup App
	App app = {0};
	app.postprocess.auto_exposure_fx.downsample_tex = tex;

	// Initialize PBOs for the test
	glGenBuffers(2, app.postprocess.histogram_pbo);
	for (int i = 0; i < 2; i++) {
		glBindBuffer(
		    GL_PIXEL_PACK_BUFFER,
		    postprocess_get_histogram_pbo(&app.postprocess, i));
		glBufferData(GL_PIXEL_PACK_BUFFER,
		             MAP_SIZE * MAP_SIZE * (GLsizeiptr)sizeof(float),
		             NULL, GL_STREAM_READ);
		postprocess_set_histogram_sync(&app.postprocess, i, NULL);
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	// Output buffers
	int buckets[HISTO_SIZE];
	float min_lum = 0.0F;
	float max_lum = 0.0F;

	// Warmup
	compute_luminance_histogram(&app, buckets, HISTO_SIZE, &min_lum,
	                            &max_lum);

	// Benchmark
	clock_t start = clock();
	for (int i = 0; i < ITERATIONS; i++) {
		app.frame_count = (uint64_t)i;
		compute_luminance_histogram(&app, buckets, HISTO_SIZE, &min_lum,
		                            &max_lum);
	}
	clock_t end = clock();
	double cpu_time_used =
	    ((double)(end - start)) / CLOCKS_PER_SEC * MS_MULTIPLIER;  // ms

	printf("Benchmark Result: %d iterations took %.2f ms (%.4f ms/call)\n",
	       ITERATIONS, cpu_time_used, cpu_time_used / ITERATIONS);

	// Final blocking wait to ensure we have data for verification
	int last_idx = (int)((ITERATIONS) % 2);
	GLsync last_sync =
	    postprocess_get_histogram_sync(&app.postprocess, last_idx);
	if (last_sync) {
		glClientWaitSync(last_sync, GL_SYNC_FLUSH_COMMANDS_BIT,
		                 1000000000);  // 1s
	}
	app.frame_count = (uint64_t)ITERATIONS;
	compute_luminance_histogram(&app, buckets, HISTO_SIZE, &min_lum,
	                            &max_lum);

	// Verify
	int total_buckets = 0;
	for (int i = 0; i < HISTO_SIZE; i++) {
		total_buckets += buckets[i];
	}
	TEST_ASSERT_GREATER_THAN(0, total_buckets);

	// Cleanup
	glDeleteTextures(1, &tex);
	glDeleteBuffers(2, app.postprocess.histogram_pbo);
	for (int i = 0; i < 2; i++) {
		GLsync current_sync =
		    postprocess_get_histogram_sync(&app.postprocess, i);
		if (current_sync) {
			glDeleteSync(current_sync);
		}
	}
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_benchmark_histogram);
	return UNITY_END();
}
