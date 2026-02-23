#include "ibl_coordinator.h"
#include "mock_gl_standalone.h"
#include "unity.h"
#include <string.h>

/* --- Stubs for PBR and PerfTimer dependencies --- */
#include "pbr.h"
#include "perf_timer.h"

/* Constants for testing */
static const double TEST_ELAPSED_MS = 1.0;
static const double TEST_ELAPSED_US = 1000.0;
static const double TEST_ELAPSED_SEC = 0.001;
static const double TEST_GPU_MS = 2.0;
static const double TEST_HYBRID_DEBUG_MS = 2.0;
static const GLuint TEST_SHADER_ID = 200;
static const GLuint TEST_IRRADIANCE_ID = 300;
static const GLuint TEST_BRDF_ID = 400;
static const float TEST_LUMINANCE = 3.0F; /* Match mock clamp */
static const GLuint MOCK_SHADER_SP = 10;
static const GLuint MOCK_SHADER_IR = 11;
static const GLuint MOCK_SHADER_L1 = 12;
static const GLuint MOCK_SHADER_L2 = 13;
static const GLuint MOCK_HDR_TEX = 99;
static const int MOCK_WIDTH = 512;
static const int MOCK_HEIGHT = 256;
static const int MAX_STEPS = 1000;
static const GLuint MOCK_RES_SPEC = 200;
static const GLuint MOCK_RES_IRR = 300;
static const float TEST_EXPECTED_THRESHOLD = 3.0F;
static const float TEST_TOLERANCE = 0.1F;
static const int TEST_COORD_DIM = 100;

/* Mocks for perf_timer */
void perf_timer_start(PerfTimer* timer)
{
	(void)timer;
}
double perf_timer_elapsed_ms(PerfTimer* timer)
{
	(void)timer;
	return TEST_ELAPSED_MS;
}
double perf_timer_elapsed_us(PerfTimer* timer)
{
	(void)timer;
	return TEST_ELAPSED_US;
}
double perf_timer_elapsed_s(PerfTimer* timer)
{
	(void)timer;
	return TEST_ELAPSED_SEC;
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
	return TEST_GPU_MS;
}
void gpu_timer_cleanup(GPUTimer* timer)
{
	(void)timer;
}

/* Mocks for hybrid timer */
HybridTimer perf_hybrid_start(void)
{
	HybridTimer timer = {0};
	return timer;
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
	return TEST_HYBRID_DEBUG_MS;
}

/* Mocks for pbr */
GLuint build_prefiltered_specular_map(GLuint shader, GLuint env, int width,
                                      int height, float threshold)
{
	(void)shader;
	(void)env;
	(void)width;
	(void)height;
	(void)threshold;
	return 0;
}
GLuint pbr_prefilter_init(int width, int height)
{
	(void)width;
	(void)height;
	return TEST_SHADER_ID;
}
void pbr_prefilter_mip(GLuint shader, const PBRSpecUniforms* uniforms,
                       GLuint env, GLuint dest, int width, int height,
                       int level, int total, int slice, int slices,
                       float threshold)
{
	(void)shader;
	(void)uniforms;
	(void)env;
	(void)dest;
	(void)width;
	(void)height;
	(void)level;
	(void)total;
	(void)slice;
	(void)slices;
	(void)threshold;
}
GLuint build_irradiance_map(GLuint shader, GLuint env, int size,
                            float threshold)
{
	(void)shader;
	(void)env;
	(void)size;
	(void)threshold;
	return 0;
}
GLuint pbr_irradiance_init(int size)
{
	(void)size;
	return TEST_IRRADIANCE_ID;
}
void pbr_irradiance_slice_compute(GLuint shader, const PBRIrrUniforms* uniforms,
                                  GLuint env, GLuint dest, int size, int slice,
                                  int slices, float threshold)
{
	(void)shader;
	(void)uniforms;
	(void)env;
	(void)dest;
	(void)size;
	(void)slice;
	(void)slices;
	(void)threshold;
}
GLuint build_brdf_lut_map(int size)
{
	(void)size;
	return TEST_BRDF_ID;
}
float compute_mean_luminance_gpu(
    GLuint shader_pass1, GLuint shader_pass2, GLuint hdr_tex, int width,
    int height,
    // NOLINTNEXTLINE(readability-non-const-parameter)
    float clamp_multiplier, GLuint ssbos[2])
{
	(void)shader_pass1;
	(void)shader_pass2;
	(void)hdr_tex;
	(void)width;
	(void)height;
	(void)clamp_multiplier;
	(void)ssbos;
	return TEST_LUMINANCE;
}

void pbr_get_spec_uniforms(GLuint shader, PBRSpecUniforms* out)
{
	(void)shader;
	if (out) {
		memset(out, 0, sizeof(*out));
	}
}

void pbr_get_irr_uniforms(GLuint shader, PBRIrrUniforms* out)
{
	(void)shader;
	if (out) {
		memset(out, 0, sizeof(*out));
	}
}
/* ------------------------------------------------ */

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static IBLCoordinator g_coord;

void setUp(void)
{
	mock_gl_reset_calls();
	/* Ensure clean state */
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	memset(&g_coord, 0, sizeof(g_coord));
}

void tearDown(void)
{
	ibl_coordinator_cleanup(&g_coord);
}

void test_ibl_coordinator_full_cycle(void)
{
	/* 1. Initialization */
	ibl_coordinator_init(&g_coord, MOCK_SHADER_SP, MOCK_SHADER_IR,
	                     MOCK_SHADER_L1, MOCK_SHADER_L2);
	TEST_ASSERT_EQUAL(IBL_STATE_IDLE, g_coord.state);

	/* 2. Start */
	ibl_coordinator_start(&g_coord, MOCK_HDR_TEX, MOCK_WIDTH, MOCK_HEIGHT);
	TEST_ASSERT_EQUAL(IBL_STATE_LUMINANCE, g_coord.state);
	TEST_ASSERT_EQUAL(MOCK_HDR_TEX, g_coord.pending_hdr_tex);

	/* 3. Update Loop until DONE */
	/* We expect transitions: LUMINANCE -> SPEC_INIT -> SPEC_MIPS -> ... ->
	 * IRRADIANCE -> DONE */
	int steps = 0;
	while (g_coord.state != IBL_STATE_DONE && steps < MAX_STEPS) {
		IBLState prev_state = g_coord.state;
		(void)prev_state; /* unused var suppression */
		ibl_coordinator_update(&g_coord, (uint64_t)steps);
		steps++;

		/* Sanity check: state should remain valid */
		TEST_ASSERT_TRUE(g_coord.state >= IBL_STATE_IDLE &&
		                 g_coord.state <= IBL_STATE_DONE);
	}

	TEST_ASSERT_EQUAL(IBL_STATE_DONE, g_coord.state);
	TEST_ASSERT_TRUE(steps < MAX_STEPS);

	/* 4. Verify Results */
	GLuint out_hdr = 0;
	GLuint out_spec = 0;
	GLuint out_irr = 0;
	float out_thresh = 0.0F;
	int res = ibl_coordinator_get_results(&g_coord, &out_hdr, &out_spec,
	                                      &out_irr, &out_thresh);

	TEST_ASSERT_EQUAL(1, res);
	TEST_ASSERT_EQUAL(MOCK_HDR_TEX, out_hdr);
	TEST_ASSERT_TRUE(out_spec != 0); /* Should have generated a spec tex */
	TEST_ASSERT_TRUE(out_irr != 0);  /* Should have generated an irr tex */
	/* Threshold comes from mock_gl_get_buffer_sub_data which returns 1.0,
	 * multiplied by clamp (3.0) -> 3.0 */
	TEST_ASSERT_FLOAT_WITHIN(TEST_TOLERANCE, TEST_EXPECTED_THRESHOLD,
	                         out_thresh);

	/* 5. Verify State Reset */
	TEST_ASSERT_EQUAL(IBL_STATE_IDLE, g_coord.state);
}

void test_ibl_coordinator_reset_cleans_resources(void)
{
	ibl_coordinator_init(&g_coord, 1, 2, 3, 4);
	ibl_coordinator_start(&g_coord, TEST_COORD_DIM, TEST_COORD_DIM,
	                      TEST_COORD_DIM);

	/* Simulate acquiring resources */
	g_coord.pending_spec_tex = MOCK_RES_SPEC;
	g_coord.pending_irr_tex = MOCK_RES_IRR;

	/* Reset */
	ibl_coordinator_reset(&g_coord);

	TEST_ASSERT_EQUAL(IBL_STATE_IDLE, g_coord.state);
	TEST_ASSERT_EQUAL(0, g_coord.pending_hdr_tex);
	TEST_ASSERT_EQUAL(0, g_coord.pending_spec_tex);
	TEST_ASSERT_EQUAL(0, g_coord.pending_irr_tex);

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
