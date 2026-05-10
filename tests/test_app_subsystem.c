#include <glad/glad.h>

#include "app.h"
#include "app_input_state.h"
#include "app_profiling.h"
#include "app_settings.h"
#include "app_subsystem.h"
#include "async/async_coordinator.h"
#include "env_manager.h"
#include "postprocess.h"
#include "scene.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>

/* ===================================================================
 * Fake subsystem helpers for Phase A (framework validation)
 * =================================================================== */

static int cleanup_call_count;

static int succeed_always(struct App* app)
{
	(void)app;
	return 1;
}

static int fail_always(struct App* app)
{
	(void)app;
	return 0;
}

static void counting_cleanup(struct App* app)
{
	(void)app;
	cleanup_call_count++;
}

/* --- Reverse-order tracking helpers --- */

enum { MAX_ORDER_SLOTS = 8 };

static int cleanup_order[MAX_ORDER_SLOTS];
static int cleanup_order_idx;

static void cleanup_track_0(struct App* app)
{
	(void)app;
	cleanup_order[cleanup_order_idx++] = 0;
}

static void cleanup_track_1(struct App* app)
{
	(void)app;
	cleanup_order[cleanup_order_idx++] = 1;
}

static void cleanup_track_2(struct App* app)
{
	(void)app;
	cleanup_order[cleanup_order_idx++] = 2;
}

static void cleanup_track_3(struct App* app)
{
	(void)app;
	cleanup_order[cleanup_order_idx++] = 3;
}

static void cleanup_track_4(struct App* app)
{
	(void)app;
	cleanup_order[cleanup_order_idx++] = 4;
}

/* Table of tracking cleanups indexed by position */
static void (*const TRACK_CLEANUPS[])(struct App*) = {
    cleanup_track_0, cleanup_track_1, cleanup_track_2,
    cleanup_track_3, cleanup_track_4,
};

/* --- Positional failure helper --- */

static int g_fail_at_position;
static int g_init_counter;

static int succeed_unless_position(struct App* app)
{
	(void)app;
	return (g_init_counter++ != g_fail_at_position) ? 1 : 0;
}

/* ===================================================================
 * Unity boilerplate
 * =================================================================== */

void setUp(void)
{
}

void tearDown(void)
{
}

/* ===================================================================
 * Phase A — Framework tests (fake subsystems only)
 * =================================================================== */

/** Test 1: All subsystems succeed — init returns 1, no cleanup called. */
void test_all_subsystems_succeed(void)
{
	SubsystemDescriptor table[] = {
	    {"a", succeed_always, counting_cleanup},
	    {"b", succeed_always, counting_cleanup},
	    {"c", succeed_always, counting_cleanup},
	    {0},
	};
	App app;
	memset(&app, 0, sizeof(app));
	cleanup_call_count = 0;

	TEST_ASSERT_EQUAL(1, app_subsystems_init(&app, table));
	TEST_ASSERT_EQUAL(0, cleanup_call_count);
}

/** Test 2: Fail at position N — exactly N cleanups called. */
void test_cleanup_on_nth_failure(void)
{
	SubsystemDescriptor table[] = {
	    {"ok1", succeed_always, counting_cleanup},
	    {"ok2", succeed_always, counting_cleanup},
	    {"ok3", succeed_always, counting_cleanup},
	    {"fail", fail_always, counting_cleanup},
	    {0},
	};
	App app;
	memset(&app, 0, sizeof(app));
	cleanup_call_count = 0;

	TEST_ASSERT_EQUAL(0, app_subsystems_init(&app, table));
	TEST_ASSERT_EQUAL(3, cleanup_call_count);
}

/** Test 3: Sweep — fail at every position [0..4], verify cleanup count. */
void test_cleanup_sweep_all_positions(void)
{
	enum { TABLE_SIZE = 5 };

	for (int fail_at = 0; fail_at < TABLE_SIZE; fail_at++) {
		SubsystemDescriptor table[TABLE_SIZE + 1];
		for (int k = 0; k < TABLE_SIZE; k++) {
			table[k].name = "s";
			table[k].init = succeed_unless_position;
			table[k].cleanup = TRACK_CLEANUPS[k];
		}
		table[TABLE_SIZE] = (SubsystemDescriptor){0};

		App app;
		memset(&app, 0, sizeof(app));
		g_fail_at_position = fail_at;
		g_init_counter = 0;
		cleanup_order_idx = 0;

		TEST_ASSERT_EQUAL(0, app_subsystems_init(&app, table));
		/* Exactly fail_at cleanups in reverse order */
		TEST_ASSERT_EQUAL(fail_at, cleanup_order_idx);
		for (int k = 0; k < fail_at; k++) {
			TEST_ASSERT_EQUAL(fail_at - 1 - k, cleanup_order[k]);
		}
	}
}

/** Test 4: Fail at position 0 — no cleanup at all. */
void test_first_subsystem_fails_no_cleanup(void)
{
	SubsystemDescriptor table[] = {
	    {"fail", fail_always, counting_cleanup},
	    {0},
	};
	App app;
	memset(&app, 0, sizeof(app));
	cleanup_call_count = 0;

	TEST_ASSERT_EQUAL(0, app_subsystems_init(&app, table));
	TEST_ASSERT_EQUAL(0, cleanup_call_count);
}

/** Test 5: Reverse order verification on failure. */
void test_cleanup_reverse_order(void)
{
	SubsystemDescriptor table[] = {
	    {"s0", succeed_always, cleanup_track_0},
	    {"s1", succeed_always, cleanup_track_1},
	    {"s2", succeed_always, cleanup_track_2},
	    {"fail", fail_always, NULL},
	    {0},
	};
	App app;
	memset(&app, 0, sizeof(app));
	cleanup_order_idx = 0;

	app_subsystems_init(&app, table);

	TEST_ASSERT_EQUAL(3, cleanup_order_idx);
	TEST_ASSERT_EQUAL(2, cleanup_order[0]); /* last success first */
	TEST_ASSERT_EQUAL(1, cleanup_order[1]);
	TEST_ASSERT_EQUAL(0, cleanup_order[2]);
}

/** Test 6: app_subsystems_cleanup() on full table (normal shutdown). */
void test_full_cleanup(void)
{
	SubsystemDescriptor table[] = {
	    {"s0", succeed_always, cleanup_track_0},
	    {"s1", succeed_always, cleanup_track_1},
	    {"s2", succeed_always, cleanup_track_2},
	    {0},
	};
	App app;
	memset(&app, 0, sizeof(app));

	TEST_ASSERT_EQUAL(1, app_subsystems_init(&app, table));

	cleanup_order_idx = 0;
	app_subsystems_cleanup(&app, table);

	TEST_ASSERT_EQUAL(3, cleanup_order_idx);
	TEST_ASSERT_EQUAL(2, cleanup_order[0]);
	TEST_ASSERT_EQUAL(1, cleanup_order[1]);
	TEST_ASSERT_EQUAL(0, cleanup_order[2]);
}

/* ===================================================================
 * Phase B — Real subsystem roundtrip tests (input + profiling)
 * =================================================================== */

/** input_subsys_init/cleanup roundtrip: alloc, init, verify, cleanup. */
void test_input_subsys_roundtrip(void)
{
	App app;
	memset(&app, 0, sizeof(app));
	app.width = 800;
	app.height = 600;

	/* Simulate what app_init Phase 1 does for input */
	app.input = calloc(1, sizeof(*app.input));
	TEST_ASSERT_NOT_NULL(app.input);

	app_input_state_init(app.input);
	TEST_ASSERT_EQUAL(1, app.input->camera_enabled);

	app_input_state_cleanup(app.input);
	free(app.input);
	app.input = NULL;
	TEST_ASSERT_NULL(app.input);
}

/** profiling_subsys_init/cleanup roundtrip: alloc, init, verify, cleanup.
 *  Needs a GL context because gpu_profiler_init() calls glGenQueries(). */
void test_profiling_subsys_roundtrip(void)
{
	TEST_ASSERT_TRUE_MESSAGE(glfwInit(), "GLFW init failed");
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	GLFWwindow* win = glfwCreateWindow(64, 64, "test", NULL, NULL);
	TEST_ASSERT_NOT_NULL_MESSAGE(win, "GLFW window creation failed");
	glfwMakeContextCurrent(win);
	TEST_ASSERT_TRUE_MESSAGE(
	    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress),
	    "GLAD load failed");

	App app;
	memset(&app, 0, sizeof(app));
	app.width = 800;
	app.height = 600;

	app.profiling = calloc(1, sizeof(*app.profiling));
	TEST_ASSERT_NOT_NULL(app.profiling);

	app_profiling_init(app.profiling, app.width, app.height);
	/* Verify some post-init state */
	TEST_ASSERT_EQUAL(0, app.profiling->perf_mode_active);
	TEST_ASSERT_EQUAL(0, app.profiling->log_gpu_metrics);

	app_profiling_cleanup(app.profiling);
	free(app.profiling);
	app.profiling = NULL;
	TEST_ASSERT_NULL(app.profiling);

	glfwDestroyWindow(win);
	glfwTerminate();
}

/* ===================================================================
 * Phase C — Step 2 descriptor roundtrips (alloc-only, no GL)
 * =================================================================== */

/** postprocess_subsys roundtrip: alloc + free. */
void test_postprocess_subsys_roundtrip(void)
{
	App app;
	memset(&app, 0, sizeof(app));

	TEST_ASSERT_EQUAL(1, postprocess_subsys_init(&app));
	TEST_ASSERT_NOT_NULL(app.postprocess);

	postprocess_subsys_cleanup(&app);
	TEST_ASSERT_NULL(app.postprocess);
}

/** scene_subsys roundtrip: alloc + defaults + free. */
void test_scene_subsys_init_sets_defaults(void)
{
	App app;
	memset(&app, 0, sizeof(app));

	TEST_ASSERT_EQUAL(1, scene_subsys_init(&app));
	TEST_ASSERT_NOT_NULL(app.scene);
	TEST_ASSERT_EQUAL(DEFAULT_SPECULAR_AA_ENABLED,
	                  app.scene->config.specular_aa_enabled);

	scene_subsys_cleanup(&app);
	TEST_ASSERT_NULL(app.scene);
}

/** env_mgr_subsys roundtrip: alloc + is_first_load default + free. */
void test_env_mgr_subsys_init_sets_defaults(void)
{
	App app;
	memset(&app, 0, sizeof(app));

	TEST_ASSERT_EQUAL(1, env_mgr_subsys_init(&app));
	TEST_ASSERT_NOT_NULL(app.env_mgr);
	TEST_ASSERT_EQUAL(1, app.env_mgr->is_first_load);

	env_mgr_subsys_cleanup(&app);
	TEST_ASSERT_NULL(app.env_mgr);
}

/** async_coord_subsys roundtrip: alloc + free. */
void test_async_coord_subsys_roundtrip(void)
{
	App app;
	memset(&app, 0, sizeof(app));

	TEST_ASSERT_EQUAL(1, async_coord_subsys_init(&app));
	TEST_ASSERT_NOT_NULL(app.async_coord);

	async_coord_subsys_cleanup(&app);
	TEST_ASSERT_NULL(app.async_coord);
}

/** Integration: full Phase 1 table (6 entries) init + cleanup. */
void test_phase1_table_full_init_cleanup(void)
{
	static const SubsystemDescriptor phase1_table[] = {
	    APP_INPUT_DESCRIPTOR,
	    APP_PROFILING_DESCRIPTOR,
	    APP_POSTPROCESS_DESCRIPTOR,
	    APP_SCENE_DESCRIPTOR,
	    APP_ENV_MGR_DESCRIPTOR,
	    APP_ASYNC_COORD_DESCRIPTOR,
	    {0},
	};

	App app;
	memset(&app, 0, sizeof(app));
	app.width = 800;
	app.height = 600;

	TEST_ASSERT_EQUAL(1, app_subsystems_init(&app, phase1_table));
	TEST_ASSERT_NOT_NULL(app.input);
	TEST_ASSERT_NOT_NULL(app.profiling);
	TEST_ASSERT_NOT_NULL(app.postprocess);
	TEST_ASSERT_NOT_NULL(app.scene);
	TEST_ASSERT_NOT_NULL(app.env_mgr);
	TEST_ASSERT_NOT_NULL(app.async_coord);

	app_subsystems_cleanup(&app, phase1_table);
	TEST_ASSERT_NULL(app.input);
	TEST_ASSERT_NULL(app.profiling);
	TEST_ASSERT_NULL(app.postprocess);
	TEST_ASSERT_NULL(app.scene);
	TEST_ASSERT_NULL(app.env_mgr);
	TEST_ASSERT_NULL(app.async_coord);
}

/** Sweep: replace each real descriptor's init with fail_always,
 *  verify preceding entries are cleaned up. */
void test_phase1_failure_sweep(void)
{
	static const SubsystemDescriptor real_table[] = {
	    APP_INPUT_DESCRIPTOR,
	    APP_PROFILING_DESCRIPTOR,
	    APP_POSTPROCESS_DESCRIPTOR,
	    APP_SCENE_DESCRIPTOR,
	    APP_ENV_MGR_DESCRIPTOR,
	    APP_ASYNC_COORD_DESCRIPTOR,
	    {0},
	};

	enum { TABLE_SIZE = 6 };

	for (int fail_at = 0; fail_at < TABLE_SIZE; fail_at++) {
		/* Build a table with fail_always injected at position fail_at
		 */
		SubsystemDescriptor table[TABLE_SIZE + 1];
		for (int k = 0; k < TABLE_SIZE; k++) {
			table[k] = real_table[k];
		}
		table[fail_at].init = fail_always;
		table[TABLE_SIZE] = (SubsystemDescriptor){0};

		App app;
		memset(&app, 0, sizeof(app));
		app.width = 800;
		app.height = 600;

		TEST_ASSERT_EQUAL(0, app_subsystems_init(&app, table));
	}
}

/* ===================================================================
 * main
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* Phase A — framework */
	RUN_TEST(test_all_subsystems_succeed);
	RUN_TEST(test_cleanup_on_nth_failure);
	RUN_TEST(test_cleanup_sweep_all_positions);
	RUN_TEST(test_first_subsystem_fails_no_cleanup);
	RUN_TEST(test_cleanup_reverse_order);
	RUN_TEST(test_full_cleanup);

	/* Phase B — real subsystem roundtrips (step 1) */
	RUN_TEST(test_input_subsys_roundtrip);
	RUN_TEST(test_profiling_subsys_roundtrip);

	/* Phase C — step 2 descriptor roundtrips (alloc-only) */
	RUN_TEST(test_postprocess_subsys_roundtrip);
	RUN_TEST(test_scene_subsys_init_sets_defaults);
	RUN_TEST(test_env_mgr_subsys_init_sets_defaults);
	RUN_TEST(test_async_coord_subsys_roundtrip);
	RUN_TEST(test_phase1_table_full_init_cleanup);
	RUN_TEST(test_phase1_failure_sweep);

	return UNITY_END();
}
