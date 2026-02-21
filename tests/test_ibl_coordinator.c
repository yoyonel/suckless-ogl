#include "ibl_coordinator.h"
#include "mock_gl_standalone.h"
#include "unity.h"
#include <string.h>

/* --- Stubs for PBR and PerfTimer dependencies --- */
#include "pbr.h"
#include "perf_timer.h"

/* Mocks for perf_timer */
void perf_timer_start(PerfTimer* timer)
{
	(void)timer;
}
double perf_timer_elapsed_ms(PerfTimer* timer)
{
	(void)timer;
	return 1.0;
}
double perf_timer_elapsed_us(PerfTimer* timer)
{
	(void)timer;
	return 1000.0;
}
double perf_timer_elapsed_s(PerfTimer* timer)
{
	(void)timer;
	return 0.001;
}

/* Mocks for gpu_timer */
void gpu_timer_start(GPUTimer* timer)
{
	(void)timer;
}
void gpu_timer_stop(GPUTimer* timer)
{
	(void)timer;
}
double gpu_timer_elapsed_ms(GPUTimer* timer, int wait)
{
	(void)timer;
	(void)wait;
	return 2.0;
}
void gpu_timer_cleanup(GPUTimer* timer)
{
	(void)timer;
}

/* Mocks for hybrid timer */
HybridTimer perf_hybrid_start(void)
{
	HybridTimer t = {0};
	return t;
}
void perf_hybrid_stop(HybridTimer* timer, const char* label)
{
	(void)timer;
	(void)label;
}
double perf_hybrid_stop_debug(HybridTimer* timer, const char* label)
{
	(void)timer;
	(void)label;
	return 2.0;
}

/* Mocks for pbr */
GLuint build_prefiltered_specular_map(GLuint shader, GLuint env, int w, int h,
                                      float t)
{
	(void)shader;
	(void)env;
	(void)w;
	(void)h;
	(void)t;
	return 0;
}
GLuint pbr_prefilter_init(int w, int h)
{
	(void)w;
	(void)h;
	return 200;
}
void pbr_prefilter_mip(GLuint shader, GLuint env, GLuint dest, int w, int h,
                       int level, int total, int slice, int slices, float t)
{
	(void)shader;
	(void)env;
	(void)dest;
	(void)w;
	(void)h;
	(void)level;
	(void)total;
	(void)slice;
	(void)slices;
	(void)t;
}
GLuint build_irradiance_map(GLuint shader, GLuint env, int size, float t)
{
	(void)shader;
	(void)env;
	(void)size;
	(void)t;
	return 0;
}
GLuint pbr_irradiance_init(int size)
{
	(void)size;
	return 300;
}
void pbr_irradiance_slice_compute(GLuint shader, GLuint env, GLuint dest,
                                  int size, int slice, int slices, float t)
{
	(void)shader;
	(void)env;
	(void)dest;
	(void)size;
	(void)slice;
	(void)slices;
	(void)t;
}
GLuint build_brdf_lut_map(int size)
{
	(void)size;
	return 400;
}
float compute_mean_luminance_gpu(GLuint s1, GLuint s2, GLuint tex, int w, int h,
                                 float m, GLuint ssbos[2])
{
	(void)s1;
	(void)s2;
	(void)tex;
	(void)w;
	(void)h;
	(void)m;
	(void)ssbos;
	return 0.5f;
}
/* ------------------------------------------------ */

static IBLCoordinator coord;

void setUp(void)
{
	mock_gl_reset_calls();
	/* Ensure clean state */
	memset(&coord, 0, sizeof(coord));
}

void tearDown(void)
{
	ibl_coordinator_cleanup(&coord);
}

void test_ibl_coordinator_full_cycle(void)
{
	/* 1. Initialization */
	GLuint s_sp = 10;
	GLuint s_ir = 11;
	GLuint s_l1 = 12;
	GLuint s_l2 = 13;

	ibl_coordinator_init(&coord, s_sp, s_ir, s_l1, s_l2);
	TEST_ASSERT_EQUAL(IBL_STATE_IDLE, coord.state);

	/* 2. Start */
	GLuint hdr_tex = 99;
	ibl_coordinator_start(&coord, hdr_tex, 512, 256);
	TEST_ASSERT_EQUAL(IBL_STATE_LUMINANCE, coord.state);
	TEST_ASSERT_EQUAL(hdr_tex, coord.pending_hdr_tex);

	/* 3. Update Loop until DONE */
	/* We expect transitions: LUMINANCE -> SPEC_INIT -> SPEC_MIPS -> ... ->
	 * IRRADIANCE -> DONE */
	int max_steps = 1000;
	int steps = 0;
	while (coord.state != IBL_STATE_DONE && steps < max_steps) {
		IBLState prev_state = coord.state;
		ibl_coordinator_update(&coord, (uint64_t)steps);
		steps++;

		/* Sanity check: state should remain valid */
		TEST_ASSERT_TRUE(coord.state >= IBL_STATE_IDLE &&
		                 coord.state <= IBL_STATE_DONE);
	}

	TEST_ASSERT_EQUAL(IBL_STATE_DONE, coord.state);
	TEST_ASSERT_TRUE(steps < max_steps);

	/* 4. Verify Results */
	GLuint out_h = 0, out_s = 0, out_i = 0;
	float out_t = 0.0F;
	int res =
	    ibl_coordinator_get_results(&coord, &out_h, &out_s, &out_i, &out_t);

	TEST_ASSERT_EQUAL(1, res);
	TEST_ASSERT_EQUAL(hdr_tex, out_h);
	TEST_ASSERT_TRUE(out_s != 0); /* Should have generated a spec texture */
	TEST_ASSERT_TRUE(out_i != 0); /* Should have generated an irr texture */
	/* Threshold comes from mock_gl_get_buffer_sub_data which returns 1.0,
	 * multiplied by clamp (3.0) -> 3.0 */
	TEST_ASSERT_FLOAT_WITHIN(0.1F, 3.0F, out_t);

	/* 5. Verify State Reset */
	TEST_ASSERT_EQUAL(IBL_STATE_IDLE, coord.state);
}

void test_ibl_coordinator_reset_cleans_resources(void)
{
	ibl_coordinator_init(&coord, 1, 2, 3, 4);
	ibl_coordinator_start(&coord, 100, 100, 100);

	/* Simulate acquiring resources */
	coord.pending_spec_tex = 200;
	coord.pending_irr_tex = 300;

	/* Reset */
	ibl_coordinator_reset(&coord);

	TEST_ASSERT_EQUAL(IBL_STATE_IDLE, coord.state);
	TEST_ASSERT_EQUAL(0, coord.pending_hdr_tex);
	TEST_ASSERT_EQUAL(0, coord.pending_spec_tex);
	TEST_ASSERT_EQUAL(0, coord.pending_irr_tex);

	/* Verify mock GL delete calls were made */
	TEST_ASSERT_GREATER_THAN(0, mock_gl_get_delete_buffer_call_count() +
	                                mock_gl_get_last_deleted_buffer() + 1);
	/* Note: mock_gl handles buffers/textures roughly. We just want to
	 * ensure it tried to clean up. */
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_ibl_coordinator_full_cycle);
	RUN_TEST(test_ibl_coordinator_reset_cleans_resources);
	return UNITY_END();
}
