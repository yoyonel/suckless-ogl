#include <glad/glad.h>

#include "app.h"
#include "app_env.h"
#include "app_settings.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
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
	g_test_app->transition_duration = TRANSITION_DURATION;
	g_test_app->env_transition_mode = ENV_TRANSITION_CROSSFADE;
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
	g_test_app->transition_state = TRANSITION_WAIT_IBL;
	g_test_app->transition_alpha = 1.0F;
	g_test_app->ibl_ctx.state = IBL_STATE_DONE;

	/* Simulate state machine processing */
	app_process_ibl_state_machine(g_test_app);

	/* Should move to FADE_IN */
	TEST_ASSERT_EQUAL(TRANSITION_FADE_IN, g_test_app->transition_state);
}

/**
 * @brief Test Crossfade mode state flow.
 */
void test_transition_crossfade_flow(void)
{
	g_test_app->env_transition_mode = ENV_TRANSITION_CROSSFADE;
	g_test_app->transition_state = TRANSITION_LOADING;
	g_test_app->ibl_ctx.state = IBL_STATE_DONE;

	/* Simulate state machine processing */
	app_process_ibl_state_machine(g_test_app);

	/* In Crossfade, Done -> FADE_IN immediately */
	TEST_ASSERT_EQUAL(TRANSITION_FADE_IN, g_test_app->transition_state);
	TEST_ASSERT_EQUAL_FLOAT(1.0F, g_test_app->transition_alpha);
	/* Snapshot texture should have been generated */
	TEST_ASSERT_NOT_EQUAL(TEXTURE_ID_ZERO,
	                      g_test_app->transition_snapshot_tex);
}

/**
 * @brief Test Black Screen mode state flow.
 */
void test_transition_black_screen_flow(void)
{
	g_test_app->env_transition_mode = ENV_TRANSITION_BLACK_SCREEN;
	g_test_app->transition_state = TRANSITION_LOADING;
	g_test_app->ibl_ctx.state = IBL_STATE_DONE;

	/* Simulate state machine processing */
	app_process_ibl_state_machine(g_test_app);

	/* In Black Screen, Done -> FADE_OUT */
	TEST_ASSERT_EQUAL(TRANSITION_FADE_OUT, g_test_app->transition_state);
	TEST_ASSERT_EQUAL_FLOAT(0.0F, g_test_app->transition_alpha);

	/* Simulate Fade Out completion */
	static const double FADE_OUT_DURATION_FACTOR = 2.0;
	g_test_app->delta_time =
	    (double)TRANSITION_DURATION * FADE_OUT_DURATION_FACTOR;
	app_update_transition(g_test_app);

	/* Should move to FADE_IN after swap */
	TEST_ASSERT_EQUAL(TRANSITION_FADE_IN, g_test_app->transition_state);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_transition_initial_state);
	RUN_TEST(test_transition_crossfade_flow);
	RUN_TEST(test_transition_black_screen_flow);
	return UNITY_END();
}
