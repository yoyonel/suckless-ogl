#include <glad/glad.h>

#include "gpu_profiler.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static GLFWwindow* test_window = NULL;

static const int TEST_WINDOW_WIDTH = 640;
static const int TEST_WINDOW_HEIGHT = 480;
static const uint32_t COLOR_RED = 0xFF0000;
static const uint32_t COLOR_GREEN = 0x00FF00;
static const int TEST_LOOP_COUNT = 5;
static const float CLEAR_COLOR_VAL = 0.1F;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	// Hidden window for headless testing
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	test_window = glfwCreateWindow(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT,
	                               "Test Window", NULL, NULL);
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

/**
 * @brief test_gpu_profiler_init
 *
 * Checks the initial state of the profiler.
 * 1. Calls gpu_profiler_init.
 * 2. Verifies that stage_count is 0.
 * 3. Verifies that write_index starts at 0.
 * Ensures the module starts with a clean structure.
 */
void test_gpu_profiler_init(void)
{
	GPUProfiler profiler;
	gpu_profiler_init(&profiler);

	TEST_ASSERT_EQUAL(0, profiler.stage_count);
	TEST_ASSERT_EQUAL(0, profiler.write_index);

	gpu_profiler_cleanup(&profiler);
}

/**
 * @brief test_gpu_profiler_double_buffering_swap
 *
 * Validates the Double Buffering (Ping-Pong) logic.
 * 1. Manually forces write=0 and read=1.
 * 2. Calls gpu_profiler_begin_frame and checks that indices swap (write=1,
 * read=0).
 * 3. Calls gpu_profiler_begin_frame again and checks they return to initial
 * state (write=0). Critical to ensure we write current frame queries while
 * reading previous frame results.
 */
void test_gpu_profiler_double_buffering_swap(void)
{
	GPUProfiler profiler;
	gpu_profiler_init(&profiler);

	// Manually setup indices to verify swap logic
	profiler.write_index = 0;
	profiler.read_index = 1;

	// This function simulates frame start and should swap buffers
	gpu_profiler_begin_frame(&profiler);

	TEST_ASSERT_EQUAL(1, profiler.write_index);
	TEST_ASSERT_EQUAL(0, profiler.read_index);

	gpu_profiler_begin_frame(&profiler);

	TEST_ASSERT_EQUAL(0, profiler.write_index);
	TEST_ASSERT_EQUAL(1, profiler.read_index);

	gpu_profiler_cleanup(&profiler);
}

/**
 * @brief test_gpu_profiler_stage_registration
 *
 * Verifies that start_stage calls correctly register stages in the list.
 * 1. Starts "Stage A", checks stage_count=1 and name "Stage A".
 * 2. Ends stage, starts "Stage B", checks stage_count=2 and name "Stage B".
 * Ensures internal data structure fills up correctly in call order.
 */
void test_gpu_profiler_stage_registration(void)
{
	GPUProfiler profiler;
	gpu_profiler_init(&profiler);

	gpu_profiler_start_stage(&profiler, "Stage A", COLOR_RED);

	// Should have added a stage
	TEST_ASSERT_EQUAL(1, profiler.stage_count);
	TEST_ASSERT_EQUAL_STRING("Stage A", profiler.stages[0].name);

	gpu_profiler_end_stage(&profiler);

	gpu_profiler_start_stage(&profiler, "Stage B", COLOR_GREEN);
	TEST_ASSERT_EQUAL(2, profiler.stage_count);
	TEST_ASSERT_EQUAL_STRING("Stage B", profiler.stages[1].name);
	gpu_profiler_end_stage(&profiler);

	gpu_profiler_cleanup(&profiler);
}

/**
 * @brief test_gpu_profiler_result_retrieval
 *
 * Verifies the complete flow: Registration -> GPU Execution -> Result
 * Retrieval.
 * 1. Runs a loop of 5 simulated frames with real GL work (glClear).
 * 2. Uses glFinish (test only) to ensure GPU completion before reading.
 * 3. Calls begin_frame to trigger query result reading.
 * 4. Verifies that the associated sampler has collected data (count > 0).
 * 5. Verifies calculated time offset is valid (>= 0.0).
 * Proves integration of glBeginQuery/glEndQuery/glGetQueryObject and storage in
 * AdaptiveSampler.
 */
void test_gpu_profiler_result_retrieval(void)
{
	GPUProfiler profiler;
	gpu_profiler_init(&profiler);

	// Simulate 5 frames
	for (int i = 0; i < TEST_LOOP_COUNT; ++i) {
		gpu_profiler_begin_frame(&profiler);

		gpu_profiler_start_stage(&profiler, "Render", COLOR_RED);

		// Do some dummy GL work
		glClearColor(CLEAR_COLOR_VAL, CLEAR_COLOR_VAL, CLEAR_COLOR_VAL,
		             1.0F);
		glClear(GL_COLOR_BUFFER_BIT);

		gpu_profiler_end_stage(&profiler);

		glFinish();
	}

	gpu_profiler_begin_frame(&profiler);

	// Check if sample count increased
	// Use .count, not .sample_count
	TEST_ASSERT_GREATER_THAN(0, profiler.stages[0].sampler.count);
	// Check offset is valid (>= 0)
	TEST_ASSERT_GREATER_OR_EQUAL(0.0, profiler.stages[0].start_offset_ms);

	gpu_profiler_cleanup(&profiler);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gpu_profiler_init);
	RUN_TEST(test_gpu_profiler_double_buffering_swap);
	RUN_TEST(test_gpu_profiler_stage_registration);
	RUN_TEST(test_gpu_profiler_result_retrieval);
	return UNITY_END();
}
