/**
 * @file test_shockwave.c
 * @brief Tests for shockwave.c — emit, update, eviction, init/cleanup.
 *
 * Uses standalone mocks (no GPU required). Includes shockwave.c directly
 * and mocks shader API + GL calls not covered by mock_gl_standalone.
 */

#include "mock_gl_standalone.h"

/* Type-providing headers */
#include "shockwave.h"
#include "unity.h"
#include <cglm/mat4.h>
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

void glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel,
                        GLint srcX, GLint srcY, GLint srcZ, GLuint dstName,
                        GLenum dstTarget, GLint dstLevel, GLint dstX,
                        GLint dstY, GLint dstZ, GLsizei srcWidth,
                        GLsizei srcHeight, GLsizei srcDepth)
{
	(void)srcName;
	(void)srcTarget;
	(void)srcLevel;
	(void)srcX;
	(void)srcY;
	(void)srcZ;
	(void)dstName;
	(void)dstTarget;
	(void)dstLevel;
	(void)dstX;
	(void)dstY;
	(void)dstZ;
	(void)srcWidth;
	(void)srcHeight;
	(void)srcDepth;
}

void glDepthMask(GLboolean flag)
{
	(void)flag;
}

/* ---- Include source under test ---- */
#include "shockwave.c"

/* ---- Helpers ---- */

static void reset_mocks(void)
{
	mock_gl_reset_calls();
	mock_shader_load_calls = 0;
	mock_shader_destroy_calls = 0;
	mock_shader_use_calls = 0;
	mock_shader_load_fail = false;
}

static ShockwaveRenderer make_renderer(void)
{
	ShockwaveRenderer r;
	memset(&r, 0, sizeof(r));
	return r;
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
 * Init / Cleanup tests
 * ===========================================================================*/

void test_init_success(void)
{
	ShockwaveRenderer r = make_renderer();
	bool ok = shockwave_renderer_init(&r);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_NOT_NULL(r.shader);
	TEST_ASSERT_EQUAL_INT(1, mock_shader_load_calls);
	/* VAO/VBO should be allocated */
	TEST_ASSERT_NOT_EQUAL(0, r.vao);
	TEST_ASSERT_NOT_EQUAL(0, r.vbo);
}

void test_init_shader_failure(void)
{
	ShockwaveRenderer r = make_renderer();
	mock_shader_load_fail = true;

	bool ok = shockwave_renderer_init(&r);

	TEST_ASSERT_FALSE(ok);
	TEST_ASSERT_NULL(r.shader);
}

void test_cleanup_releases_resources(void)
{
	ShockwaveRenderer r = make_renderer();
	shockwave_renderer_init(&r);
	reset_mocks();

	shockwave_renderer_cleanup(&r);

	TEST_ASSERT_EQUAL_INT(1, mock_shader_destroy_calls);
	TEST_ASSERT_EQUAL(0, r.vbo);
	TEST_ASSERT_EQUAL(0, r.vao);
	TEST_ASSERT_NULL(r.shader);
}

void test_cleanup_with_grab_texture(void)
{
	ShockwaveRenderer r = make_renderer();
	shockwave_renderer_init(&r);
	/* Simulate grab texture allocation */
	r.grab_tex = 42;
	reset_mocks();

	shockwave_renderer_cleanup(&r);

	TEST_ASSERT_EQUAL(0, r.grab_tex);
}

/* ===========================================================================
 * Emit tests
 * ===========================================================================*/

void test_emit_below_min_velocity_ignored(void)
{
	ShockwaveRenderer r = make_renderer();
	vec3 pos = {1.0F, 2.0F, 3.0F};
	vec3 col = {1.0F, 0.0F, 0.0F};

	shockwave_emit(&r, pos, col, SHOCKWAVE_MIN_VELOCITY * 0.5F, 0.0F);

	TEST_ASSERT_EQUAL_INT(0, r.count);
}

void test_emit_at_min_velocity_ignored(void)
{
	ShockwaveRenderer r = make_renderer();
	vec3 pos = {1.0F, 2.0F, 3.0F};
	vec3 col = {1.0F, 0.0F, 0.0F};

	/* Exactly at threshold — should still be below (< not <=) */
	shockwave_emit(&r, pos, col, SHOCKWAVE_MIN_VELOCITY - 0.001F, 0.0F);

	TEST_ASSERT_EQUAL_INT(0, r.count);
}

void test_emit_above_min_velocity_creates_event(void)
{
	ShockwaveRenderer r = make_renderer();
	vec3 pos = {1.0F, 2.0F, 3.0F};
	vec3 col = {1.0F, 0.5F, 0.0F};

	shockwave_emit(&r, pos, col, 1.0F, 10.0F);

	TEST_ASSERT_EQUAL_INT(1, r.count);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 1.0F, r.events[0].position[0]);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 2.0F, r.events[0].position[1]);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 3.0F, r.events[0].position[2]);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 10.0F, r.events[0].start_time);
}

void test_emit_intensity_clamped_to_one(void)
{
	ShockwaveRenderer r = make_renderer();
	vec3 pos = {0};
	vec3 col = {0};

	/* Very high velocity → intensity should cap at 1.0 */
	shockwave_emit(&r, pos, col, 100.0F, 0.0F);

	TEST_ASSERT_FLOAT_WITHIN(0.01F, 1.0F, r.events[0].intensity);
}

void test_emit_fills_buffer(void)
{
	ShockwaveRenderer r = make_renderer();
	vec3 pos = {0};
	vec3 col = {1, 1, 1};

	for (int i = 0; i < SHOCKWAVE_MAX_ACTIVE; i++) {
		pos[0] = (float)i;
		shockwave_emit(&r, pos, col, 1.0F, (float)i);
	}

	TEST_ASSERT_EQUAL_INT(SHOCKWAVE_MAX_ACTIVE, r.count);
}

void test_emit_evicts_oldest_when_full(void)
{
	ShockwaveRenderer r = make_renderer();
	vec3 pos = {0};
	vec3 col = {1, 1, 1};

	/* Fill buffer — event 0 has start_time=0.0 (oldest) */
	for (int i = 0; i < SHOCKWAVE_MAX_ACTIVE; i++) {
		shockwave_emit(&r, pos, col, 1.0F, (float)i);
	}

	/* Emit one more — should evict event with start_time=0.0 */
	vec3 new_pos = {99.0F, 99.0F, 99.0F};
	shockwave_emit(&r, new_pos, col, 1.0F, 100.0F);

	/* Count stays at max */
	TEST_ASSERT_EQUAL_INT(SHOCKWAVE_MAX_ACTIVE, r.count);

	/* The event at index 0 should now be the new one */
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 99.0F, r.events[0].position[0]);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 100.0F, r.events[0].start_time);
}

/* ===========================================================================
 * Update tests
 * ===========================================================================*/

void test_update_removes_expired_events(void)
{
	ShockwaveRenderer r = make_renderer();
	vec3 pos = {0};
	vec3 col = {1, 1, 1};

	/* Emit at t=0 */
	shockwave_emit(&r, pos, col, 1.0F, 0.0F);
	TEST_ASSERT_EQUAL_INT(1, r.count);

	/* Update at t=DURATION+1 → event should be expired */
	shockwave_update(&r, SHOCKWAVE_DURATION + 1.0F);
	TEST_ASSERT_EQUAL_INT(0, r.count);
}

void test_update_keeps_active_events(void)
{
	ShockwaveRenderer r = make_renderer();
	vec3 pos = {0};
	vec3 col = {1, 1, 1};

	/* Emit at t=0 */
	shockwave_emit(&r, pos, col, 1.0F, 0.0F);

	/* Update at t=DURATION/2 → event should still be alive */
	shockwave_update(&r, SHOCKWAVE_DURATION * 0.5F);
	TEST_ASSERT_EQUAL_INT(1, r.count);
}

void test_update_partial_expiry(void)
{
	ShockwaveRenderer r = make_renderer();
	vec3 pos = {0};
	vec3 col = {1, 1, 1};

	/* Emit 3 events at different times (sim_time will be 4.1s) */
	shockwave_emit(&r, pos, col, 1.0F, 0.0F); /* age=4.1 >= 3.6 → expired */
	shockwave_emit(&r, pos, col, 1.0F, 1.0F); /* age=3.1 <  3.6 → alive   */
	shockwave_emit(&r, pos, col, 1.0F, 2.0F); /* age=2.1 <  3.6 → alive   */
	TEST_ASSERT_EQUAL_INT(3, r.count);

	/* Update past first event's lifetime */
	shockwave_update(&r, SHOCKWAVE_DURATION + 0.5F);
	TEST_ASSERT_EQUAL_INT(2, r.count);
}

void test_update_empty_renderer_is_noop(void)
{
	ShockwaveRenderer r = make_renderer();
	shockwave_update(&r, 100.0F);
	TEST_ASSERT_EQUAL_INT(0, r.count);
}

/* ===========================================================================
 * Draw tests (mainly early-exit paths)
 * ===========================================================================*/

void test_draw_noop_when_count_zero(void)
{
	ShockwaveRenderer r = make_renderer();
	shockwave_renderer_init(&r);
	reset_mocks();

	mat4 view = GLM_MAT4_IDENTITY_INIT;
	mat4 proj = GLM_MAT4_IDENTITY_INIT;
	vec3 cam = {0, 0, 5};

	shockwave_draw(&r, view, proj, cam, 0.0F, 800, 600);

	/* Should not use shader */
	TEST_ASSERT_EQUAL_INT(0, mock_shader_use_calls);

	shockwave_renderer_cleanup(&r);
}

void test_draw_noop_when_no_shader(void)
{
	ShockwaveRenderer r = make_renderer();
	r.count = 1; /* has events but no shader */

	mat4 view = GLM_MAT4_IDENTITY_INIT;
	mat4 proj = GLM_MAT4_IDENTITY_INIT;
	vec3 cam = {0, 0, 5};

	shockwave_draw(&r, view, proj, cam, 0.0F, 800, 600);

	TEST_ASSERT_EQUAL_INT(0, mock_shader_use_calls);
}

void test_draw_with_events_uses_shader(void)
{
	ShockwaveRenderer r = make_renderer();
	shockwave_renderer_init(&r);
	vec3 pos = {0};
	vec3 col = {1, 0, 0};
	shockwave_emit(&r, pos, col, 1.0F, 0.0F);
	reset_mocks();

	mat4 view = GLM_MAT4_IDENTITY_INIT;
	mat4 proj = GLM_MAT4_IDENTITY_INIT;
	vec3 cam = {0, 0, 5};

	shockwave_draw(&r, view, proj, cam, 0.5F, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_shader_use_calls);

	shockwave_renderer_cleanup(&r);
}

/* ---- Main ---- */

int main(void)
{
	UNITY_BEGIN();
	/* Init/Cleanup */
	RUN_TEST(test_init_success);
	RUN_TEST(test_init_shader_failure);
	RUN_TEST(test_cleanup_releases_resources);
	RUN_TEST(test_cleanup_with_grab_texture);
	/* Emit */
	RUN_TEST(test_emit_below_min_velocity_ignored);
	RUN_TEST(test_emit_at_min_velocity_ignored);
	RUN_TEST(test_emit_above_min_velocity_creates_event);
	RUN_TEST(test_emit_intensity_clamped_to_one);
	RUN_TEST(test_emit_fills_buffer);
	RUN_TEST(test_emit_evicts_oldest_when_full);
	/* Update */
	RUN_TEST(test_update_removes_expired_events);
	RUN_TEST(test_update_keeps_active_events);
	RUN_TEST(test_update_partial_expiry);
	RUN_TEST(test_update_empty_renderer_is_noop);
	/* Draw early-exit */
	RUN_TEST(test_draw_noop_when_count_zero);
	RUN_TEST(test_draw_noop_when_no_shader);
	RUN_TEST(test_draw_with_events_uses_shader);
	return UNITY_END();
}
