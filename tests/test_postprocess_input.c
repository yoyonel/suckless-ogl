/**
 * @file test_postprocess_input.c
 * @brief Tests for postprocess_input.c — key handlers, toggle, presets.
 *
 * Uses standalone mocks (no GPU required). Includes postprocess_input.c
 * directly and mocks all external postprocess/notifier/benchmark APIs.
 */

#include "mock_gl_standalone.h"

/* Type-providing headers — must come before mock definitions */
#include "app_settings.h"
#include "postprocess_input.h"
#include "postprocess_internal.h"
#include "unity.h"
#include <string.h>

/* ---- Mock state ---- */

static int mock_toggle_calls;
static PostProcessEffect mock_last_toggled;

static int mock_enable_calls;
static PostProcessEffect mock_last_enabled;

static int mock_disable_calls;
static PostProcessEffect mock_last_disabled;

static int mock_set_exposure_calls;
static float mock_last_exposure_value;

static int mock_apply_preset_calls;

static int mock_load_lut3d_calls;
static int mock_load_lut3d_return;

static int mock_notifier_push_calls;
static char mock_last_notification[128];

static int mock_bench_start_calls;
static bool mock_bench_is_running_val;

static int mock_ae_toggle_path_calls;
static int mock_ae_path_name_calls;

/* ---- Mock postprocess API ---- */

void postprocess_toggle(PostProcess* pp, PostProcessEffect effect)
{
	pp->active_effects ^= (unsigned int)effect;
	mock_toggle_calls++;
	mock_last_toggled = effect;
}

void postprocess_enable(PostProcess* pp, PostProcessEffect effect)
{
	pp->active_effects |= (unsigned int)effect;
	mock_enable_calls++;
	mock_last_enabled = effect;
}

void postprocess_disable(PostProcess* pp, PostProcessEffect effect)
{
	pp->active_effects &= ~(unsigned int)effect;
	mock_disable_calls++;
	mock_last_disabled = effect;
}

int postprocess_is_enabled(PostProcess* pp, PostProcessEffect effect)
{
	return (pp->active_effects & (unsigned int)effect) != 0;
}

void postprocess_set_exposure(PostProcess* pp, float exposure)
{
	pp->exposure.exposure = exposure;
	mock_set_exposure_calls++;
	mock_last_exposure_value = exposure;
}

void postprocess_apply_preset(PostProcess* pp, const PostProcessPreset* preset)
{
	pp->active_effects = preset->active_effects;
	mock_apply_preset_calls++;
}

int postprocess_load_lut3d(PostProcess* pp, const char* path)
{
	(void)pp;
	(void)path;
	mock_load_lut3d_calls++;
	return mock_load_lut3d_return;
}

/* ---- Mock action_notifier ---- */

void action_notifier_push(ActionNotifier* notifier, const char* text,
                          float duration)
{
	(void)notifier;
	(void)duration;
	mock_notifier_push_calls++;
	strncpy(mock_last_notification, text,
	        sizeof(mock_last_notification) - 1);
	mock_last_notification[sizeof(mock_last_notification) - 1] = '\0';
}

/* ---- Mock effect_benchmark ---- */

bool effect_benchmark_start(EffectBenchmark* bench)
{
	(void)bench;
	mock_bench_start_calls++;
	return true;
}

bool effect_benchmark_is_running(const EffectBenchmark* bench)
{
	(void)bench;
	return mock_bench_is_running_val;
}

/* ---- Mock auto-exposure ---- */

void fx_auto_exposure_toggle_path(AutoExposureFX* auto_exp)
{
	(void)auto_exp;
	mock_ae_toggle_path_calls++;
}

const char* fx_auto_exposure_path_name(AutoExposureFX* auto_exp)
{
	(void)auto_exp;
	mock_ae_path_name_calls++;
	return "CPU Histogram";
}

/* ---- Include source under test ---- */
#include "postprocess_input.c"

/* ---- Helpers ---- */

static PostProcess g_pp;
static ActionNotifier g_notifier;
static EffectBenchmark g_bench;
static PostProcessInputContext g_ctx;

static void reset_mocks(void)
{
	mock_gl_reset_calls();
	mock_toggle_calls = 0;
	mock_last_toggled = 0;
	mock_enable_calls = 0;
	mock_last_enabled = 0;
	mock_disable_calls = 0;
	mock_last_disabled = 0;
	mock_set_exposure_calls = 0;
	mock_last_exposure_value = 0.0F;
	mock_apply_preset_calls = 0;
	mock_load_lut3d_calls = 0;
	mock_load_lut3d_return = 0;
	mock_notifier_push_calls = 0;
	mock_last_notification[0] = '\0';
	mock_bench_start_calls = 0;
	mock_bench_is_running_val = false;
	mock_ae_toggle_path_calls = 0;
	mock_ae_path_name_calls = 0;
}

static void setup_context(void)
{
	memset(&g_pp, 0, sizeof(g_pp));
	memset(&g_notifier, 0, sizeof(g_notifier));
	memset(&g_bench, 0, sizeof(g_bench));
	g_ctx.postprocess = &g_pp;
	g_ctx.notifier = &g_notifier;
	g_ctx.effect_bench = &g_bench;
	g_ctx.window = NULL;
}

/* ---- Unity ---- */

void setUp(void)
{
	reset_mocks();
	setup_context();
}

void tearDown(void)
{
}

/* ===========================================================================
 * Toggle tests
 * ===========================================================================*/

void test_key_v_toggles_vignette(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_V, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_VIGNETTE, mock_last_toggled);
	TEST_ASSERT_EQUAL_INT(1, mock_notifier_push_calls);
}

void test_key_g_toggles_grain(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_G, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_GRAIN, mock_last_toggled);
}

void test_key_b_toggles_bloom(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_B, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_BLOOM, mock_last_toggled);
}

void test_key_h_toggles_dof(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_H, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_DOF, mock_last_toggled);
}

void test_key_h_shift_toggles_dof_debug(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_H, GLFW_MOD_SHIFT);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_DOF_DEBUG, mock_last_toggled);
}

void test_key_u_toggles_chrom_abbr(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_U, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_CHROM_ABBR, mock_last_toggled);
}

void test_key_m_toggles_motion_blur(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_M, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_MOTION_BLUR, mock_last_toggled);
}

void test_key_x_toggles_fxaa(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_X, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_FXAA, mock_last_toggled);
}

void test_key_x_shift_toggles_fxaa_debug(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_X, GLFW_MOD_SHIFT);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_FXAA_DEBUG, mock_last_toggled);
}

void test_key_f6_toggles_stencil_debug(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_F6, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_STENCIL_DEBUG, mock_last_toggled);
}

void test_key_f7_toggles_fog(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_F7, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_FOG, mock_last_toggled);
}

void test_key_f7_shift_toggles_fog_debug(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_F7, GLFW_MOD_SHIFT);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_FOG_DEBUG, mock_last_toggled);
}

/* ===========================================================================
 * Exposure tests
 * ===========================================================================*/

void test_kp_add_increases_exposure(void)
{
	g_pp.exposure.exposure = 1.0F;

	postprocess_input_handle_key(&g_ctx, GLFW_KEY_KP_ADD, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_set_exposure_calls);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 1.0F + DEFAULT_EXPOSURE_STEP,
	                         mock_last_exposure_value);
}

void test_kp_subtract_decreases_exposure(void)
{
	g_pp.exposure.exposure = 2.0F;

	postprocess_input_handle_key(&g_ctx, GLFW_KEY_KP_SUBTRACT, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_set_exposure_calls);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, 2.0F - DEFAULT_EXPOSURE_STEP,
	                         mock_last_exposure_value);
}

void test_kp_subtract_clamps_at_min(void)
{
	g_pp.exposure.exposure = DEFAULT_MIN_EXPOSURE;

	postprocess_input_handle_key(&g_ctx, GLFW_KEY_KP_SUBTRACT, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_set_exposure_calls);
	TEST_ASSERT_FLOAT_WITHIN(0.01F, DEFAULT_MIN_EXPOSURE,
	                         mock_last_exposure_value);
}

/* ===========================================================================
 * Auto-exposure
 * ===========================================================================*/

void test_key_j_toggles_auto_exposure(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_J, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_toggle_calls);
	TEST_ASSERT_EQUAL(POSTFX_AUTO_EXPOSURE, mock_last_toggled);
}

void test_key_j_ctrl_toggles_ae_path(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_J, GLFW_MOD_CONTROL);

	TEST_ASSERT_EQUAL_INT(1, mock_ae_toggle_path_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_ae_path_name_calls);
}

/* ===========================================================================
 * Preset tests
 * ===========================================================================*/

void test_key_1_applies_default_preset(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_1, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_apply_preset_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_set_exposure_calls);
}

void test_key_2_applies_subtle_preset(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_2, 0);
	TEST_ASSERT_EQUAL_INT(1, mock_apply_preset_calls);
}

void test_key_3_applies_cinematic_preset(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_3, 0);
	TEST_ASSERT_EQUAL_INT(1, mock_apply_preset_calls);
}

void test_key_0_resets_to_defaults(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_0, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_apply_preset_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_set_exposure_calls);
}

void test_key_kp0_resets_to_defaults(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_KP_0, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_apply_preset_calls);
}

/* ===========================================================================
 * Bloom debug cycling
 * ===========================================================================*/

void test_key_b_shift_cycles_bloom_debug(void)
{
	/* First press: Off -> Debug Final */
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_B, GLFW_MOD_SHIFT);
	TEST_ASSERT_TRUE(postprocess_is_enabled(&g_pp, POSTFX_BLOOM_DEBUG));
	TEST_ASSERT_EQUAL_INT(0, g_pp.bloom_fx.debug_step);

	/* Second press: Final -> Prefilter */
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_B, GLFW_MOD_SHIFT);
	TEST_ASSERT_EQUAL_INT(1, g_pp.bloom_fx.debug_step);

	/* Third press: Prefilter -> Downsample */
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_B, GLFW_MOD_SHIFT);
	TEST_ASSERT_EQUAL_INT(2, g_pp.bloom_fx.debug_step);

	/* Fourth press: Downsample -> Upsample */
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_B, GLFW_MOD_SHIFT);
	TEST_ASSERT_EQUAL_INT(3, g_pp.bloom_fx.debug_step);

	/* Fifth press: Upsample -> Off */
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_B, GLFW_MOD_SHIFT);
	TEST_ASSERT_FALSE(postprocess_is_enabled(&g_pp, POSTFX_BLOOM_DEBUG));
}

/* ===========================================================================
 * Motion blur debug cycling
 * ===========================================================================*/

void test_key_m_shift_cycles_motion_blur_debug(void)
{
	/* First: Off -> MB Debug */
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_M, GLFW_MOD_SHIFT);
	TEST_ASSERT_TRUE(
	    postprocess_is_enabled(&g_pp, POSTFX_MOTION_BLUR_DEBUG));

	/* Second: MB Debug -> Vector Field */
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_M, GLFW_MOD_SHIFT);
	TEST_ASSERT_FALSE(
	    postprocess_is_enabled(&g_pp, POSTFX_MOTION_BLUR_DEBUG));
	TEST_ASSERT_TRUE(
	    postprocess_is_enabled(&g_pp, POSTFX_VECTOR_FIELD_DEBUG));

	/* Third: Vector Field -> Off */
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_M, GLFW_MOD_SHIFT);
	TEST_ASSERT_FALSE(
	    postprocess_is_enabled(&g_pp, POSTFX_VECTOR_FIELD_DEBUG));
}

/* ===========================================================================
 * Benchmark
 * ===========================================================================*/

void test_key_8_starts_benchmark(void)
{
	mock_bench_is_running_val = false;

	postprocess_input_handle_key(&g_ctx, GLFW_KEY_8, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_bench_start_calls);
}

void test_key_8_noop_when_benchmark_running(void)
{
	mock_bench_is_running_val = true;

	postprocess_input_handle_key(&g_ctx, GLFW_KEY_8, 0);

	TEST_ASSERT_EQUAL_INT(0, mock_bench_start_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_notifier_push_calls);
}

/* ===========================================================================
 * Banding style cycling (key 7)
 * ===========================================================================*/

void test_key_7_enables_banding_first_press(void)
{
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_7, 0);

	TEST_ASSERT_EQUAL_INT(1, mock_apply_preset_calls);
	TEST_ASSERT_EQUAL_INT(0, g_pp.banding_preset_idx);
}

void test_key_7_cycles_banding_styles(void)
{
	/* First press: enable at idx 0 */
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_7, 0);
	TEST_ASSERT_EQUAL_INT(0, g_pp.banding_preset_idx);

	/* Second press: cycle to idx 1 */
	g_pp.active_effects |= (unsigned int)POSTFX_BANDING;
	postprocess_input_handle_key(&g_ctx, GLFW_KEY_7, 0);
	TEST_ASSERT_EQUAL_INT(1, g_pp.banding_preset_idx);
}

/* ---- Main ---- */

int main(void)
{
	UNITY_BEGIN();
	/* Toggles */
	RUN_TEST(test_key_v_toggles_vignette);
	RUN_TEST(test_key_g_toggles_grain);
	RUN_TEST(test_key_b_toggles_bloom);
	RUN_TEST(test_key_h_toggles_dof);
	RUN_TEST(test_key_h_shift_toggles_dof_debug);
	RUN_TEST(test_key_u_toggles_chrom_abbr);
	RUN_TEST(test_key_m_toggles_motion_blur);
	RUN_TEST(test_key_x_toggles_fxaa);
	RUN_TEST(test_key_x_shift_toggles_fxaa_debug);
	RUN_TEST(test_key_f6_toggles_stencil_debug);
	RUN_TEST(test_key_f7_toggles_fog);
	RUN_TEST(test_key_f7_shift_toggles_fog_debug);
	/* Exposure */
	RUN_TEST(test_kp_add_increases_exposure);
	RUN_TEST(test_kp_subtract_decreases_exposure);
	RUN_TEST(test_kp_subtract_clamps_at_min);
	/* Auto-exposure */
	RUN_TEST(test_key_j_toggles_auto_exposure);
	RUN_TEST(test_key_j_ctrl_toggles_ae_path);
	/* Presets */
	RUN_TEST(test_key_1_applies_default_preset);
	RUN_TEST(test_key_2_applies_subtle_preset);
	RUN_TEST(test_key_3_applies_cinematic_preset);
	RUN_TEST(test_key_0_resets_to_defaults);
	RUN_TEST(test_key_kp0_resets_to_defaults);
	/* Bloom debug */
	RUN_TEST(test_key_b_shift_cycles_bloom_debug);
	/* Motion blur debug */
	RUN_TEST(test_key_m_shift_cycles_motion_blur_debug);
	/* Benchmark */
	RUN_TEST(test_key_8_starts_benchmark);
	RUN_TEST(test_key_8_noop_when_benchmark_running);
	/* Banding */
	RUN_TEST(test_key_7_enables_banding_first_press);
	RUN_TEST(test_key_7_cycles_banding_styles);
	return UNITY_END();
}
