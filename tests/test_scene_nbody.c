/**
 * @file test_scene_nbody.c
 * @brief Tests for scene_nbody.c — N-body scene integration (toggle + update).
 *
 * Uses standalone mocks (no GPU required). Includes scene_nbody.c directly
 * and mocks all visual/GPU subsystem calls.
 */

#include "mock_gl_standalone.h"

/* Include type-providing headers BEFORE mock definitions */
#include "billboard_rendering.h"
#include "billboard_sorting.h"
#include "instanced_rendering.h"
#include "nbody.h"
#include "scene.h"
#include "scene_internal.h"
#include "scene_simulation.h"
#include "shockwave.h"
#include "trail_renderer.h"
#include "unity.h"
#include <stdbool.h>
#include <string.h>

/* ---- Mock call counters ---- */
static int mock_trail_init_calls;
static int mock_trail_cleanup_calls;
static int mock_trail_record_calls;
static int mock_trail_set_color_calls;
static int mock_shockwave_init_calls;
static int mock_shockwave_cleanup_calls;
static int mock_shockwave_emit_calls;
static int mock_shockwave_update_calls;
static int mock_instanced_update_calls;
static int mock_instanced_cleanup_calls;
static int mock_billboard_cleanup_calls;
static int mock_billboard_sorter_cleanup_calls;
static int mock_scene_init_instancing_calls;

/* Control: make trail_renderer_init fail */
static bool mock_trail_init_fail;
/* Control: make shockwave_renderer_init fail */
static bool mock_shockwave_init_fail;

/* ---- Mocks for visual/GPU subsystems ---- */

bool trail_renderer_init(TrailRenderer* tr, int body_count)
{
	(void)tr;
	(void)body_count;
	mock_trail_init_calls++;
	return !mock_trail_init_fail;
}

void trail_renderer_cleanup(TrailRenderer* tr)
{
	(void)tr;
	mock_trail_cleanup_calls++;
}

void trail_renderer_record(TrailRenderer* tr, const NBodySim* sim,
                           float delta_time)
{
	(void)tr;
	(void)sim;
	(void)delta_time;
	mock_trail_record_calls++;
}

void trail_renderer_set_color(TrailRenderer* tr, int body_index,
                              const float color[3])
{
	(void)tr;
	(void)body_index;
	(void)color;
	mock_trail_set_color_calls++;
}

bool shockwave_renderer_init(ShockwaveRenderer* renderer)
{
	(void)renderer;
	mock_shockwave_init_calls++;
	return !mock_shockwave_init_fail;
}

void shockwave_renderer_cleanup(ShockwaveRenderer* renderer)
{
	(void)renderer;
	mock_shockwave_cleanup_calls++;
}

void shockwave_emit(ShockwaveRenderer* renderer, const vec3 position,
                    const vec3 color, float velocity, float sim_time)
{
	(void)renderer;
	(void)position;
	(void)color;
	(void)velocity;
	(void)sim_time;
	mock_shockwave_emit_calls++;
}

void shockwave_update(ShockwaveRenderer* renderer, float sim_time)
{
	(void)renderer;
	(void)sim_time;
	mock_shockwave_update_calls++;
}

void instanced_group_update(InstancedGroup* group, const SphereInstance* data,
                            int count)
{
	(void)group;
	(void)data;
	(void)count;
	mock_instanced_update_calls++;
}

void instanced_group_cleanup(InstancedGroup* group)
{
	(void)group;
	mock_instanced_cleanup_calls++;
}

void billboard_group_cleanup(BillboardGroup* group)
{
	(void)group;
	mock_billboard_cleanup_calls++;
}

void billboard_sorter_cleanup(BillboardSorter* sorter)
{
	(void)sorter;
	mock_billboard_sorter_cleanup_calls++;
}

void scene_init_instancing(Scene* scene)
{
	(void)scene;
	mock_scene_init_instancing_calls++;
}

/* ---- Include source under test ---- */
#include "scene_nbody.c"

/* ---- Helpers ---- */

static void reset_mocks(void)
{
	mock_gl_reset_calls();
	mock_trail_init_calls = 0;
	mock_trail_cleanup_calls = 0;
	mock_trail_record_calls = 0;
	mock_trail_set_color_calls = 0;
	mock_shockwave_init_calls = 0;
	mock_shockwave_cleanup_calls = 0;
	mock_shockwave_emit_calls = 0;
	mock_shockwave_update_calls = 0;
	mock_instanced_update_calls = 0;
	mock_instanced_cleanup_calls = 0;
	mock_billboard_cleanup_calls = 0;
	mock_billboard_sorter_cleanup_calls = 0;
	mock_scene_init_instancing_calls = 0;
	mock_trail_init_fail = false;
	mock_shockwave_init_fail = false;
}

static SceneSimulation test_simulation;

static Scene make_test_scene(void)
{
	Scene scene;
	memset(&scene, 0, sizeof(scene));
	memset(&test_simulation, 0, sizeof(test_simulation));
	scene.simulation = &test_simulation;
	return scene;
}

/* ---- Unity Setup/Teardown ---- */

void setUp(void)
{
	reset_mocks();
}

void tearDown(void)
{
}

/* ---- Tests: scene_toggle_nbody ---- */

void test_toggle_nbody_enables_mode(void)
{
	Scene scene = make_test_scene();
	scene.simulation->nbody_mode = 0;

	scene_toggle_nbody(&scene);

	TEST_ASSERT_TRUE(scene.simulation->nbody_mode);
	TEST_ASSERT_EQUAL_INT(1, mock_trail_init_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_shockwave_init_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_instanced_update_calls);
	TEST_ASSERT_TRUE(mock_trail_set_color_calls > 0);
}

void test_toggle_nbody_disables_mode(void)
{
	Scene scene = make_test_scene();
	/* Enable first */
	scene.simulation->nbody_mode = 0;
	scene_toggle_nbody(&scene);
	reset_mocks();

	/* Now toggle off */
	scene_toggle_nbody(&scene);

	TEST_ASSERT_FALSE(scene.simulation->nbody_mode);
	TEST_ASSERT_EQUAL_INT(1, mock_trail_cleanup_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_shockwave_cleanup_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_instanced_cleanup_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_billboard_cleanup_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_scene_init_instancing_calls);
}

void test_toggle_nbody_trail_init_failure_aborts(void)
{
	Scene scene = make_test_scene();
	scene.simulation->nbody_mode = 0;
	mock_trail_init_fail = true;

	scene_toggle_nbody(&scene);

	/* Mode should stay off */
	TEST_ASSERT_FALSE(scene.simulation->nbody_mode);
	/* shockwave_init should NOT have been called */
	TEST_ASSERT_EQUAL_INT(0, mock_shockwave_init_calls);
}

void test_toggle_nbody_shockwave_init_failure_cleans_trails(void)
{
	Scene scene = make_test_scene();
	scene.simulation->nbody_mode = 0;
	mock_shockwave_init_fail = true;

	scene_toggle_nbody(&scene);

	/* Mode should stay off */
	TEST_ASSERT_FALSE(scene.simulation->nbody_mode);
	/* Trail was inited then cleaned up */
	TEST_ASSERT_EQUAL_INT(1, mock_trail_init_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_trail_cleanup_calls);
}

void test_toggle_nbody_roundtrip(void)
{
	Scene scene = make_test_scene();

	/* Start off */
	TEST_ASSERT_FALSE(scene.simulation->nbody_mode);

	/* Enable */
	scene_toggle_nbody(&scene);
	TEST_ASSERT_TRUE(scene.simulation->nbody_mode);

	/* Disable */
	scene_toggle_nbody(&scene);
	TEST_ASSERT_FALSE(scene.simulation->nbody_mode);

	/* Enable again */
	scene_toggle_nbody(&scene);
	TEST_ASSERT_TRUE(scene.simulation->nbody_mode);
}

/* ---- Tests: scene_nbody_update ---- */

void test_nbody_update_noop_when_disabled(void)
{
	Scene scene = make_test_scene();
	scene.simulation->nbody_mode = 0;

	scene_nbody_update(&scene, 1.0F / 60.0F);

	/* Nothing should be called */
	TEST_ASSERT_EQUAL_INT(0, mock_shockwave_update_calls);
	TEST_ASSERT_EQUAL_INT(0, mock_trail_record_calls);
	TEST_ASSERT_EQUAL_INT(0, mock_instanced_update_calls);
}

void test_nbody_update_calls_subsystems(void)
{
	Scene scene = make_test_scene();
	scene.simulation->nbody_mode = 0;
	scene_toggle_nbody(&scene);
	reset_mocks();

	scene_nbody_update(&scene, 1.0F / 60.0F);

	TEST_ASSERT_EQUAL_INT(1, mock_shockwave_update_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_trail_record_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_instanced_update_calls);
}

void test_nbody_update_multiple_steps(void)
{
	Scene scene = make_test_scene();
	scene.simulation->nbody_mode = 0;
	scene_toggle_nbody(&scene);
	reset_mocks();

	enum { STEP_COUNT = 10 };

	for (int i = 0; i < STEP_COUNT; i++) {
		scene_nbody_update(&scene, 1.0F / 60.0F);
	}

	TEST_ASSERT_EQUAL_INT(STEP_COUNT, mock_shockwave_update_calls);
	TEST_ASSERT_EQUAL_INT(STEP_COUNT, mock_trail_record_calls);
	TEST_ASSERT_EQUAL_INT(STEP_COUNT, mock_instanced_update_calls);
}

/* ---- Main ---- */

int main(void)
{
	UNITY_BEGIN();
	/* scene_toggle_nbody */
	RUN_TEST(test_toggle_nbody_enables_mode);
	RUN_TEST(test_toggle_nbody_disables_mode);
	RUN_TEST(test_toggle_nbody_trail_init_failure_aborts);
	RUN_TEST(test_toggle_nbody_shockwave_init_failure_cleans_trails);
	RUN_TEST(test_toggle_nbody_roundtrip);
	/* scene_nbody_update */
	RUN_TEST(test_nbody_update_noop_when_disabled);
	RUN_TEST(test_nbody_update_calls_subsystems);
	RUN_TEST(test_nbody_update_multiple_steps);
	return UNITY_END();
}
