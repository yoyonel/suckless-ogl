#include <glad/glad.h>

#include "app.h"
#include "app_settings.h"
#include "env_manager.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>

static App* g_test_app = NULL;

static const int WINDOW_WIDTH = 640;
static const int WINDOW_HEIGHT = 480;
static const float TRANSITION_DURATION = 0.1F;
static const GLuint TEXTURE_ID_ZERO = 0;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	g_test_app = malloc(sizeof(App));
	memset(g_test_app, 0, sizeof(App));

	g_test_app->window =
	    glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Test", NULL, NULL);
	if (!g_test_app->window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create GLFW window");
	}
	glfwMakeContextCurrent(g_test_app->window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(g_test_app->window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to initialize GLAD");
	}

	g_test_app->width = WINDOW_WIDTH;
	g_test_app->height = WINDOW_HEIGHT;
	g_test_app->env_mgr.transition_duration = TRANSITION_DURATION;
	g_test_app->env_mgr.env_transition_mode = ENV_TRANSITION_CROSSFADE;
}

void tearDown(void)
{
	if (g_test_app->window) {
		glfwDestroyWindow(g_test_app->window);
	}
	free(g_test_app);
	glfwTerminate();
}

/**
 * @brief Test initial state and first load transition.
 */
void test_transition_initial_state(void)
{
	g_test_app->env_mgr.transition_state = TRANSITION_WAIT_IBL;
	g_test_app->env_mgr.transition_alpha = 1.0F;
	g_test_app->scene.ibl_coord.state = IBL_STATE_DONE;

	/* Simulate state machine processing */
	env_manager_update_ibl(
	    &g_test_app->env_mgr, &g_test_app->scene, &g_test_app->postprocess,
	    &g_test_app->auto_threshold, g_test_app->frame_count,
	    g_test_app->width, g_test_app->height);

	/* Should move to FADE_IN */
	TEST_ASSERT_EQUAL(TRANSITION_FADE_IN,
	                  g_test_app->env_mgr.transition_state);
}

/**
 * @brief Test Crossfade mode state flow.
 */
void test_transition_crossfade_flow(void)
{
	g_test_app->env_mgr.env_transition_mode = ENV_TRANSITION_CROSSFADE;
	g_test_app->env_mgr.transition_state = TRANSITION_LOADING;
	g_test_app->scene.ibl_coord.state = IBL_STATE_DONE;

	/* Simulate state machine processing */
	env_manager_update_ibl(
	    &g_test_app->env_mgr, &g_test_app->scene, &g_test_app->postprocess,
	    &g_test_app->auto_threshold, g_test_app->frame_count,
	    g_test_app->width, g_test_app->height);

	/* In Crossfade, Done -> FADE_IN immediately */
	TEST_ASSERT_EQUAL(TRANSITION_FADE_IN,
	                  g_test_app->env_mgr.transition_state);
	TEST_ASSERT_EQUAL_FLOAT(1.0F, g_test_app->env_mgr.transition_alpha);
	/* Snapshot texture should have been generated */
	TEST_ASSERT_NOT_EQUAL(TEXTURE_ID_ZERO,
	                      g_test_app->scene.transition_snapshot_tex);
}

/**
 * @brief Test Black Screen mode state flow.
 */
void test_transition_black_screen_flow(void)
{
	g_test_app->env_mgr.env_transition_mode = ENV_TRANSITION_BLACK_SCREEN;
	g_test_app->env_mgr.transition_state = TRANSITION_LOADING;
	g_test_app->scene.ibl_coord.state = IBL_STATE_DONE;

	/* Simulate state machine processing */
	env_manager_update_ibl(
	    &g_test_app->env_mgr, &g_test_app->scene, &g_test_app->postprocess,
	    &g_test_app->auto_threshold, g_test_app->frame_count,
	    g_test_app->width, g_test_app->height);

	/* In Black Screen, Done -> FADE_OUT */
	TEST_ASSERT_EQUAL(TRANSITION_FADE_OUT,
	                  g_test_app->env_mgr.transition_state);
	TEST_ASSERT_EQUAL_FLOAT(0.0F, g_test_app->env_mgr.transition_alpha);

	/* Simulate Fade Out completion */
	static const double FADE_OUT_DURATION_FACTOR = 2.0;
	g_test_app->delta_time =
	    (double)TRANSITION_DURATION * FADE_OUT_DURATION_FACTOR;
	env_manager_update_transition(
	    &g_test_app->env_mgr, &g_test_app->scene, &g_test_app->postprocess,
	    &g_test_app->auto_threshold, g_test_app->delta_time, 0);

	/* Should move to FADE_IN after swap */
	TEST_ASSERT_EQUAL(TRANSITION_FADE_IN,
	                  g_test_app->env_mgr.transition_state);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_transition_initial_state);
	RUN_TEST(test_transition_crossfade_flow);
	RUN_TEST(test_transition_black_screen_flow);
	return UNITY_END();
}
