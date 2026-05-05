#include <glad/glad.h>

#include "gpu_profiler.h"
#include "postprocess_internal.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>

static GLFWwindow* test_window = NULL;
static GPUProfiler dummy_profiler;

static const int RENDER_WIDTH = 640;
static const int RENDER_HEIGHT = 480;
static const int MAP_SIZE = 64;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	test_window =
	    glfwCreateWindow(RENDER_WIDTH, RENDER_HEIGHT, "Test", NULL, NULL);
	if (!test_window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create window");
	}
	glfwMakeContextCurrent(test_window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	gpu_profiler_init(&dummy_profiler);
}

void tearDown(void)
{
	gpu_profiler_cleanup(&dummy_profiler);
	if (test_window) {
		glfwDestroyWindow(test_window);
	}
	glfwTerminate();
}

/**
 * Test standard histogram computation and cache verification
 */
void test_histogram_caching_and_continuity(void)
{
	// USE HEAP for PostProcess to avoid stack issues
	PostProcess* post_proc = (PostProcess*)calloc(1, sizeof(PostProcess));
	postprocess_init(post_proc, &dummy_profiler, RENDER_WIDTH,
	                 RENDER_HEIGHT);
	postprocess_enable(post_proc, POSTFX_EXPOSURE_DEBUG);

	int buckets[POSTPROCESS_HISTOGRAM_BUCKETS] = {0};
	float min_lum = 0.0F;
	float max_lum = 0.0F;

	// 1. Initial State: sync objects are NULL
	int result = postprocess_compute_luminance_histogram(
	    post_proc, 0, buckets, POSTPROCESS_HISTOGRAM_BUCKETS, &min_lum,
	    &max_lum);
	TEST_ASSERT_EQUAL_INT(0, result);

	// 2. Mock PBO content for Frame 1 (reads from index 1)
	int write_idx = 1;
	glBindBuffer(GL_PIXEL_PACK_BUFFER,
	             post_proc->readback.histogram_pbo[write_idx]);
	float* mock_data =
	    (float*)malloc((size_t)(MAP_SIZE * MAP_SIZE) * sizeof(float));
	for (int i = 0; i < MAP_SIZE * MAP_SIZE; i++) {
		mock_data[i] = -5.0F;  // Maps to bucket 0
	}
	glBufferSubData(
	    GL_PIXEL_PACK_BUFFER, 0,
	    (GLsizeiptr)((size_t)(MAP_SIZE * MAP_SIZE) * sizeof(float)),
	    mock_data);
	free(mock_data);
	post_proc->readback.histogram_sync[write_idx] =
	    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	glFlush();
	glFinish();  // Ensure data is ready

	// 3. Process data for Frame 1
	result = postprocess_compute_luminance_histogram(
	    post_proc, 1, buckets, POSTPROCESS_HISTOGRAM_BUCKETS, &min_lum,
	    &max_lum);
	TEST_ASSERT_EQUAL_INT(1, result);
	TEST_ASSERT_EQUAL_INT(MAP_SIZE * MAP_SIZE, buckets[0]);

	// 4. Test Cache Continuity for Frame 2
	// Frame 1 auto-triggered a readback for index 0 (Frame 2).
	// In the test, we mock a slow GPU by either deleting the sync OR
	// replacing it with a non-signaled one.
	// Simplest: clear the sync to simulate it's NOT ready.
	if (post_proc->readback.histogram_sync[0]) {
		glDeleteSync(post_proc->readback.histogram_sync[0]);
		post_proc->readback.histogram_sync[0] = NULL;
	}

	result = postprocess_compute_luminance_histogram(
	    post_proc, 2, buckets, POSTPROCESS_HISTOGRAM_BUCKETS, &min_lum,
	    &max_lum);
	TEST_ASSERT_EQUAL_INT(1, result);  // Still returns 1 from cache
	TEST_ASSERT_EQUAL_INT(MAP_SIZE * MAP_SIZE, buckets[0]);

	postprocess_cleanup(post_proc);
	free(post_proc);
}

void test_histogram_auto_trigger_when_ae_off(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &dummy_profiler, RENDER_WIDTH,
	                 RENDER_HEIGHT);
	postprocess_enable(&post_proc, POSTFX_EXPOSURE_DEBUG);

	GLuint dummy_tex;
	glGenTextures(1, &dummy_tex);
	glBindTexture(GL_TEXTURE_2D, dummy_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, RENDER_WIDTH, RENDER_HEIGHT,
	             0, GL_RGBA, GL_FLOAT, NULL);
	post_proc.gpu.scene_color_tex = dummy_tex;

	postprocess_end(&post_proc);
	TEST_ASSERT_NOT_NULL(post_proc.readback.histogram_sync[1]);

	glDeleteTextures(1, &dummy_tex);
	postprocess_cleanup(&post_proc);
}

void test_histogram_cache_no_accumulation(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &dummy_profiler, RENDER_WIDTH,
	                 RENDER_HEIGHT);
	postprocess_enable(&post_proc, POSTFX_EXPOSURE_DEBUG);

	int buckets[POSTPROCESS_HISTOGRAM_BUCKETS] = {0};
	float min_lum = 0.0F;
	float max_lum = 0.0F;

	// Fill index 1 with -5.0f
	int write_idx = 1;
	glBindBuffer(GL_PIXEL_PACK_BUFFER,
	             post_proc.readback.histogram_pbo[write_idx]);
	float* data1 =
	    (float*)malloc((size_t)(MAP_SIZE * MAP_SIZE) * sizeof(float));
	for (int i = 0; i < MAP_SIZE * MAP_SIZE; i++)
		data1[i] = -5.0F;
	glBufferSubData(
	    GL_PIXEL_PACK_BUFFER, 0,
	    (GLsizeiptr)((size_t)(MAP_SIZE * MAP_SIZE) * sizeof(float)), data1);
	free(data1);
	post_proc.readback.histogram_sync[write_idx] =
	    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	glFlush();
	glFinish();

	postprocess_compute_luminance_histogram(&post_proc, 1, buckets,
	                                        POSTPROCESS_HISTOGRAM_BUCKETS,
	                                        &min_lum, &max_lum);
	TEST_ASSERT_EQUAL_INT(MAP_SIZE * MAP_SIZE, buckets[0]);

	// Load 5.0F into index 0
	write_idx = 0;
	glBindBuffer(GL_PIXEL_PACK_BUFFER,
	             post_proc.readback.histogram_pbo[write_idx]);
	float* data2 =
	    (float*)malloc((size_t)(MAP_SIZE * MAP_SIZE) * sizeof(float));
	for (int i = 0; i < MAP_SIZE * MAP_SIZE; i++)
		data2[i] = 5.0F;
	glBufferSubData(
	    GL_PIXEL_PACK_BUFFER, 0,
	    (GLsizeiptr)((size_t)(MAP_SIZE * MAP_SIZE) * sizeof(float)), data2);
	free(data2);
	// Delete any auto-triggered sync to avoid interference
	if (post_proc.readback.histogram_sync[write_idx]) {
		glDeleteSync(post_proc.readback.histogram_sync[write_idx]);
	}
	post_proc.readback.histogram_sync[write_idx] =
	    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	glFlush();
	glFinish();

	postprocess_compute_luminance_histogram(&post_proc, 2, buckets,
	                                        POSTPROCESS_HISTOGRAM_BUCKETS,
	                                        &min_lum, &max_lum);
	TEST_ASSERT_EQUAL_INT(0, buckets[0]);  // No accumulation
	TEST_ASSERT_EQUAL_INT(MAP_SIZE * MAP_SIZE,
	                      buckets[POSTPROCESS_HISTOGRAM_BUCKETS - 1]);

	postprocess_cleanup(&post_proc);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_histogram_caching_and_continuity);
	RUN_TEST(test_histogram_auto_trigger_when_ae_off);
	RUN_TEST(test_histogram_cache_no_accumulation);
	return UNITY_END();
}
