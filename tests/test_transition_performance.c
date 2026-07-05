// tests/test_transition_performance.c
#define _POSIX_C_SOURCE 200809L
#include <glad/glad.h>

#include "app.h"
#include "app_input_state.h"
#include "app_profiling.h"
#include "camera.h"
#include "env_manager.h"
#include "postprocess_internal.h"
#include "renderer.h"
#include "scene.h"
#include "scene_gpu_resources.h"
#include "scene_shaders.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const int WINDOW_WIDTH = 320;
static const int WINDOW_HEIGHT = 240;

static App g_test_app;
static bool g_app_initialized = false;

static PFNGLMEMORYBARRIERPROC original_glMemoryBarrier = NULL;
static int glMemoryBarrier_count = 0;

static void hooked_glMemoryBarrier(GLbitfield barriers)
{
	if (g_app_initialized && g_test_app.env_mgr &&
	    g_test_app.env_mgr->env_transition_mode ==
	        ENV_TRANSITION_BLACK_SCREEN &&
	    (g_test_app.env_mgr->transition_state == TRANSITION_FADE_OUT ||
	     g_test_app.env_mgr->transition_state == TRANSITION_FADE_IN)) {
		printf(
		    "[glMemoryBarrier Hook] Call detected with barriers: "
		    "0x%X\n",
		    (unsigned int)barriers);
		if (barriers == GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) {
			glMemoryBarrier_count++;
			printf(
			    "[glMemoryBarrier Hook] Incremented "
			    "glMemoryBarrier_count to %d\n",
			    glMemoryBarrier_count);
		}
	}

	if (original_glMemoryBarrier) {
		original_glMemoryBarrier(barriers);
	}
}

void setUp(void)
{
	if (!g_app_initialized) {
		if (!glfwInit()) {
			TEST_FAIL_MESSAGE("Failed to initialize GLFW");
		}
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

		int result = app_init(&g_test_app, WINDOW_WIDTH, WINDOW_HEIGHT,
		                      "Transition Perf Test");
		if (result != 1) {
			glfwTerminate();
			TEST_FAIL_MESSAGE("Failed to initialize App");
		}
		g_app_initialized = true;

		// Hook glMemoryBarrier using GLAD's function pointer
		original_glMemoryBarrier = glad_glMemoryBarrier;
		glad_glMemoryBarrier = hooked_glMemoryBarrier;
	}
	glMemoryBarrier_count = 0;
}

void tearDown(void)
{
	// Cleanup is done in main after all tests
}

void test_black_screen_transition_performance(void)
{
	TEST_ASSERT_TRUE(g_app_initialized);

	// Wait for the initial HDR load to complete and transition to become
	// IDLE
	int timeout = 1000;
	double last_time = glfwGetTime();
	while ((g_test_app.scene->gpu->hdr_texture == 0 ||
	        g_test_app.env_mgr->transition_state != TRANSITION_IDLE) &&
	       timeout-- > 0) {
		double current_time = glfwGetTime();
		double dt = current_time - last_time;
		last_time = current_time;

		// Cap delta_time to avoid large spikes during initialization
		if (dt > 0.033) {
			dt = 0.016;
		}

		g_test_app.delta_time = dt;
		app_update(&g_test_app);

		RenderContext rctx = {
		    .scene = g_test_app.scene,
		    .postprocess = g_test_app.postprocess,
		    .camera = &g_test_app.input->camera,
		    .profiler = &g_test_app.profiling->gpu_profiler,
		    .profiler_ui = &g_test_app.profiling->timeline_ui,
		    .env_mgr = g_test_app.env_mgr,
		    .notifier = &g_test_app.notifier,
		    .effect_bench = &g_test_app.effect_bench,
		    .width = g_test_app.width,
		    .height = g_test_app.height,
		    .delta_time = g_test_app.delta_time,
		    .frame_count = g_test_app.frame_count,
		    .log_gpu_metrics = g_test_app.profiling->log_gpu_metrics,
		};
		renderer_draw_frame(&rctx);

		glfwSwapBuffers(g_test_app.win.handle);
		glfwPollEvents();

		struct timespec req = {0, 5000000L};  // 5ms sleep
		nanosleep(&req, NULL);
	}

	TEST_ASSERT_TRUE_MESSAGE(g_test_app.scene->gpu->hdr_texture != 0,
	                         "Initial HDR texture failed to load");
	TEST_ASSERT_EQUAL(TRANSITION_IDLE,
	                  g_test_app.env_mgr->transition_state);

	// Reset memory barrier count before active transition
	glMemoryBarrier_count = 0;

	// Trigger transition in ENV_TRANSITION_BLACK_SCREEN mode
	g_test_app.env_mgr->env_transition_mode = ENV_TRANSITION_BLACK_SCREEN;
	g_test_app.env_mgr->transition_duration =
	    0.1f;  // Short duration for fast testing

	int success = env_manager_trigger_transition(
	    g_test_app.env_mgr, g_test_app.async_loader, "axis_test.ktx2");
	TEST_ASSERT_TRUE_MESSAGE(success, "Failed to trigger transition");

	// Run the transition until it finishes (becomes IDLE again)
	timeout = 2000;
	last_time = glfwGetTime();

	double max_frametime_ms = 0.0;
	int transition_frames_count = 0;

	while (g_test_app.env_mgr->transition_state != TRANSITION_IDLE &&
	       timeout-- > 0) {
		double current_time = glfwGetTime();
		double dt = current_time - last_time;
		last_time = current_time;

		// Limit the dt to actual elapsed time but with a cap if system
		// lag is huge
		double frame_start = glfwGetTime();

		g_test_app.delta_time = (dt > 0.05) ? 0.016 : dt;

		app_update(&g_test_app);

		RenderContext rctx = {
		    .scene = g_test_app.scene,
		    .postprocess = g_test_app.postprocess,
		    .camera = &g_test_app.input->camera,
		    .profiler = &g_test_app.profiling->gpu_profiler,
		    .profiler_ui = &g_test_app.profiling->timeline_ui,
		    .env_mgr = g_test_app.env_mgr,
		    .notifier = &g_test_app.notifier,
		    .effect_bench = &g_test_app.effect_bench,
		    .width = g_test_app.width,
		    .height = g_test_app.height,
		    .delta_time = g_test_app.delta_time,
		    .frame_count = g_test_app.frame_count,
		    .log_gpu_metrics = g_test_app.profiling->log_gpu_metrics,
		};
		renderer_draw_frame(&rctx);

		glfwSwapBuffers(g_test_app.win.handle);
		glfwPollEvents();

		double frame_duration = glfwGetTime() - frame_start;
		double frame_duration_ms = frame_duration * 1000.0;

		// Track metrics only during active fade phases (fade out & fade
		// in)
		if (g_test_app.env_mgr->transition_state ==
		        TRANSITION_FADE_OUT ||
		    g_test_app.env_mgr->transition_state ==
		        TRANSITION_FADE_IN) {
			transition_frames_count++;
			if (frame_duration_ms > max_frametime_ms) {
				max_frametime_ms = frame_duration_ms;
			}
		}

		struct timespec req = {0, 5000000L};  // 5ms sleep
		nanosleep(&req, NULL);
	}

	TEST_ASSERT_EQUAL(TRANSITION_IDLE,
	                  g_test_app.env_mgr->transition_state);
	TEST_ASSERT_TRUE(transition_frames_count > 0);

	printf("\n=== PERFORMANCE TEST RESULTS ===\n");
	printf("Transition mode: ENV_TRANSITION_BLACK_SCREEN\n");
	printf("Active transition frames: %d\n", transition_frames_count);
	printf("Max frametime recorded: %.3f ms\n", max_frametime_ms);
	printf("glMemoryBarrier calls during transition: %d\n",
	       glMemoryBarrier_count);
	printf("=================================\n");

	// 1. Assert frametime is maintained < 16.6ms via the internal metrics
	double avg_frametime_ms =
	    g_test_app.profiling->fps_counter.average_frame_time * 1000.0;
	TEST_ASSERT_TRUE_MESSAGE(avg_frametime_ms < 16.6,
	                         "Average frametime exceeded 16.6ms");

	// 2. Assert no glMemoryBarrier call is emitted during the active fade
	// phases of transition
	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    0, glMemoryBarrier_count,
	    "glMemoryBarrier was called during active transition!");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_black_screen_transition_performance);

	if (g_app_initialized) {
		glad_glMemoryBarrier = original_glMemoryBarrier;
		app_cleanup(&g_test_app);
		glfwTerminate();
	}
	return UNITY_END();
}
