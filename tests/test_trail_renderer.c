/**
 * @file test_trail_renderer.c
 * @brief Tests for trail_renderer.c — ring buffer, record, duration,
 * init/cleanup.
 *
 * Uses standalone mocks (no GPU required). Includes trail_renderer.c directly
 * and mocks shader API + GL calls not covered by mock_gl_standalone.
 */

#include "mock_gl_standalone.h"

/* Type-providing headers */
#include "shader.h"
#include "trail_renderer.h"
#include "unity.h"
#include <math.h>
#include <string.h>

/* ---- Mock shader API ---- */

static int mock_shader_load_calls;
static int mock_shader_destroy_calls;
static int mock_shader_use_calls;
static bool mock_shader_load_fail;
static Shader g_mock_shader;

Shader* shader_load(const char* vertex_path, const char* fragment_path)
{
	(void)vertex_path;
	(void)fragment_path;
	mock_shader_load_calls++;
	if (mock_shader_load_fail) {
		return NULL;
	}
	return &g_mock_shader;
}

void shader_destroy(Shader* shader)
{
	(void)shader;
	mock_shader_destroy_calls++;
}

void shader_use(Shader* shader)
{
	(void)shader;
	mock_shader_use_calls++;
}

void shader_set_int(Shader* shader, const char* name, int val)
{
	(void)shader;
	(void)name;
	(void)val;
}

void shader_set_float(Shader* shader, const char* name, float val)
{
	(void)shader;
	(void)name;
	(void)val;
}

void shader_set_vec3(Shader* shader, const char* name, const float* val)
{
	(void)shader;
	(void)name;
	(void)val;
}

void shader_set_mat4(Shader* shader, const char* name, const float* val)
{
	(void)shader;
	(void)name;
	(void)val;
}

/* ---- Extra GL mocks not in mock_gl_standalone ---- */

void glMultiDrawArrays(GLenum mode, const GLint* first,
                       const GLsizei* count_arr, GLsizei drawcount)
{
	(void)mode;
	(void)first;
	(void)count_arr;
	(void)drawcount;
}

void glCullFace(GLenum mode2)
{
	(void)mode2;
}

/* ---- Include source under test ---- */
#include "trail_renderer.c"

/* ---- Helpers ---- */

static void reset_mocks(void)
{
	mock_gl_reset_calls();
	mock_shader_load_calls = 0;
	mock_shader_destroy_calls = 0;
	mock_shader_use_calls = 0;
	mock_shader_load_fail = false;
}

/* ---- Unity ---- */

void setUp(void)
{
	reset_mocks();
}

void tearDown(void)
{
}

/* ===========================================================================
 * Init / Cleanup
 * ===========================================================================*/

void test_init_success(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));

	bool ok = trail_renderer_init(&tr, 4);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_NOT_NULL(tr.shader);
	TEST_ASSERT_EQUAL_INT(4, tr.body_count);
	TEST_ASSERT_NOT_EQUAL(0, tr.vao);
	TEST_ASSERT_NOT_EQUAL(0, tr.vbo);
	/* Default neon params */
	TEST_ASSERT_FLOAT_WITHIN(0.01F, TRAIL_NEON_INTENSITY_DEFAULT,
	                         tr.neon.intensity);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, TRAIL_NEON_CORE_EXP_DEFAULT,
	                         tr.neon.core_exp);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, TRAIL_NEON_WIDTH_DEFAULT,
	                         tr.neon.width);
	/* Default duration */
	TEST_ASSERT_FLOAT_WITHIN(0.01F, TRAIL_DURATION_DEFAULT,
	                         tr.trail_duration);

	trail_renderer_cleanup(&tr);
}

void test_init_shader_failure(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	mock_shader_load_fail = true;

	bool ok = trail_renderer_init(&tr, 4);

	TEST_ASSERT_FALSE(ok);
	TEST_ASSERT_NULL(tr.shader);
}

void test_cleanup_releases_resources(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 2);
	reset_mocks();

	trail_renderer_cleanup(&tr);

	TEST_ASSERT_EQUAL_INT(1, mock_shader_destroy_calls);
	TEST_ASSERT_NULL(tr.shader);
	TEST_ASSERT_EQUAL(0, tr.vbo);
	TEST_ASSERT_EQUAL(0, tr.vao);
}

/* ===========================================================================
 * Set color
 * ===========================================================================*/

void test_set_color_valid_index(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	vec3 color = {1.0F, 0.5F, 0.2F};

	trail_renderer_set_color(&tr, 0, color);

	TEST_ASSERT_FLOAT_WITHIN(0.01F, 1.0F, tr.colors[0][0]);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 0.5F, tr.colors[0][1]);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 0.2F, tr.colors[0][2]);
}

void test_set_color_out_of_bounds_ignored(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	vec3 color = {1.0F, 1.0F, 1.0F};

	/* Negative index */
	trail_renderer_set_color(&tr, -1, color);
	/* Out of bounds */
	trail_renderer_set_color(&tr, NBODY_MAX_BODIES, color);

	/* Should not crash — colors should remain zero */
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 0.0F, tr.colors[0][0]);
}

/* ===========================================================================
 * Clear
 * ===========================================================================*/

void test_clear_resets_rings(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 2);

	/* Push some data */
	NBodySim sim;
	memset(&sim, 0, sizeof(sim));
	sim.body_count = 2;
	sim.time_scale = 1.0F;

	trail_renderer_record(&tr, &sim, TRAIL_SAMPLE_INTERVAL);

	trail_renderer_clear(&tr);

	TEST_ASSERT_EQUAL_INT(0, tr.rings[0].count);
	TEST_ASSERT_EQUAL_INT(0, tr.rings[0].head);
	TEST_ASSERT_EQUAL_INT(0, tr.rings[1].count);
	TEST_ASSERT_EQUAL_INT(0, tr.rings[1].head);
	TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.0F, tr.sample_timer);

	trail_renderer_cleanup(&tr);
}

/* ===========================================================================
 * Duration accessors
 * ===========================================================================*/

void test_get_duration_default(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);

	TEST_ASSERT_FLOAT_WITHIN(0.01F, TRAIL_DURATION_DEFAULT,
	                         trail_renderer_get_duration(&tr));
	trail_renderer_cleanup(&tr);
}

void test_set_duration_valid(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);

	trail_renderer_set_duration(&tr, 10.0F);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 10.0F,
	                         trail_renderer_get_duration(&tr));

	trail_renderer_cleanup(&tr);
}

void test_set_duration_clamp_min(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);

	trail_renderer_set_duration(&tr, 0.01F);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, TRAIL_DURATION_MIN,
	                         trail_renderer_get_duration(&tr));

	trail_renderer_cleanup(&tr);
}

void test_set_duration_clamp_max(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);

	trail_renderer_set_duration(&tr, 999.0F);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, TRAIL_DURATION_MAX,
	                         trail_renderer_get_duration(&tr));

	trail_renderer_cleanup(&tr);
}

/* ===========================================================================
 * Record
 * ===========================================================================*/

void test_record_accumulates_sample_timer(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);

	NBodySim sim;
	memset(&sim, 0, sizeof(sim));
	sim.body_count = 1;
	sim.time_scale = 1.0F;

	/* Record with delta_time less than sample interval → no sample yet */
	trail_renderer_record(&tr, &sim, TRAIL_SAMPLE_INTERVAL * 0.5F);
	TEST_ASSERT_EQUAL_INT(0, tr.rings[0].count);

	/* Another half-interval → should now have 1 sample */
	trail_renderer_record(&tr, &sim, TRAIL_SAMPLE_INTERVAL * 0.5F);
	TEST_ASSERT_EQUAL_INT(1, tr.rings[0].count);

	trail_renderer_cleanup(&tr);
}

void test_record_respects_time_scale(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);

	NBodySim sim;
	memset(&sim, 0, sizeof(sim));
	sim.body_count = 1;
	sim.time_scale = 2.0F; /* Double speed */

	/* delta=interval/2 but time_scale=2 → effective = interval */
	trail_renderer_record(&tr, &sim, TRAIL_SAMPLE_INTERVAL * 0.5F);
	TEST_ASSERT_EQUAL_INT(1, tr.rings[0].count);

	trail_renderer_cleanup(&tr);
}

void test_record_negative_time_scale_uses_abs(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);

	NBodySim sim;
	memset(&sim, 0, sizeof(sim));
	sim.body_count = 1;
	sim.time_scale = -1.0F; /* Time reversal */

	trail_renderer_record(&tr, &sim, TRAIL_SAMPLE_INTERVAL);
	TEST_ASSERT_EQUAL_INT(1, tr.rings[0].count);

	trail_renderer_cleanup(&tr);
}

void test_record_large_dt_produces_multiple_samples(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);

	NBodySim sim;
	memset(&sim, 0, sizeof(sim));
	sim.body_count = 1;
	sim.time_scale = 1.0F;

	/* Large dt = 3× sample interval → should produce 3 samples */
	trail_renderer_record(&tr, &sim, TRAIL_SAMPLE_INTERVAL * 3.0F);
	TEST_ASSERT_EQUAL_INT(3, tr.rings[0].count);

	trail_renderer_cleanup(&tr);
}

/* ===========================================================================
 * Ring buffer wrap-around
 * ===========================================================================*/

void test_ring_wraps_at_max_points(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);

	NBodySim sim;
	memset(&sim, 0, sizeof(sim));
	sim.body_count = 1;
	sim.time_scale = 1.0F;

	/* Push TRAIL_MAX_POINTS + 10 samples */
	int total = TRAIL_MAX_POINTS + 10;
	for (int i = 0; i < total; i++) {
		trail_renderer_record(&tr, &sim, TRAIL_SAMPLE_INTERVAL);
	}

	/* Count should cap at TRAIL_MAX_POINTS */
	TEST_ASSERT_EQUAL_INT(TRAIL_MAX_POINTS, tr.rings[0].count);

	trail_renderer_cleanup(&tr);
}

/* ===========================================================================
 * Draw early-exit
 * ===========================================================================*/

void test_draw_noop_when_no_samples(void)
{
	TrailRenderer tr;
	memset(&tr, 0, sizeof(tr));
	trail_renderer_init(&tr, 1);
	reset_mocks();

	mat4 view = GLM_MAT4_IDENTITY_INIT;
	mat4 proj = GLM_MAT4_IDENTITY_INIT;
	vec3 cam = {0, 0, 5};

	trail_renderer_draw(&tr, view, proj, cam);

	/* No shader use since no vertices to draw */
	TEST_ASSERT_EQUAL_INT(0, mock_shader_use_calls);

	trail_renderer_cleanup(&tr);
}

/* ---- Main ---- */

int main(void)
{
	UNITY_BEGIN();
	/* Init/Cleanup */
	RUN_TEST(test_init_success);
	RUN_TEST(test_init_shader_failure);
	RUN_TEST(test_cleanup_releases_resources);
	/* Set color */
	RUN_TEST(test_set_color_valid_index);
	RUN_TEST(test_set_color_out_of_bounds_ignored);
	/* Clear */
	RUN_TEST(test_clear_resets_rings);
	/* Duration */
	RUN_TEST(test_get_duration_default);
	RUN_TEST(test_set_duration_valid);
	RUN_TEST(test_set_duration_clamp_min);
	RUN_TEST(test_set_duration_clamp_max);
	/* Record */
	RUN_TEST(test_record_accumulates_sample_timer);
	RUN_TEST(test_record_respects_time_scale);
	RUN_TEST(test_record_negative_time_scale_uses_abs);
	RUN_TEST(test_record_large_dt_produces_multiple_samples);
	/* Ring buffer */
	RUN_TEST(test_ring_wraps_at_max_points);
	/* Draw */
	RUN_TEST(test_draw_noop_when_no_samples);
	return UNITY_END();
}
