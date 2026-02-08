#include <glad/glad.h>

#include "gpu_profiler.h"  // INDISPENSABLE: Pour la définition de GPUProfiler
#include "postprocess.h"
#include "postprocess_presets.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <time.h>

static const int TestWidth = 640;
static const int TestHeight = 480;
static const int SmallTestWidth = 100;
static const int SmallTestHeight = 100;
static const int NewDimension = 200;
static const float TestEpsilon = 1e-5F;
static const float DefaultNeutral = 1.0F;
static const int GL_VER_MAJOR = 3;
static const int GL_VER_MINOR = 3;
static const GLuint GL_INVALID = 0;
static const int TEX_LEVEL_0 = 0;
static const int LOOP_COUNT_20 = 20;
static const int LOOP_COUNT_10 = 10;
static const unsigned int LOOP_COUNT_32 = 32;
static const unsigned int LOOP_COUNT_40 = 40;
static const float TIME_THRESHOLD = 0.05F;

static GLFWwindow* test_window = NULL;
// Instance globale pour les tests, initialisée dans setUp
static GPUProfiler gpu_profiler_system;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_VER_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_VER_MINOR);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	test_window =
	    glfwCreateWindow(TestWidth, TestHeight, "Test Window", NULL, NULL);
	if (!test_window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create GLFW window");
	}

	glfwMakeContextCurrent(test_window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(test_window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to initialize GLAD");
	}

	// Initialisation du profiler pour ce test
	gpu_profiler_init(&gpu_profiler_system);
}

void tearDown(void)
{
	// Nettoyage impératif pour éviter les fuites de samplers (ASAN)
	gpu_profiler_cleanup(&gpu_profiler_system);

	if (test_window) {
		glfwDestroyWindow(test_window);
	}
	glfwTerminate();
}

void test_postprocess_init_creates_resources(void)
{
	PostProcess post_proc = {0};
	// Injection du profiler system
	int result = postprocess_init(&post_proc, &gpu_profiler_system,
	                              TestWidth, TestHeight);

	TEST_ASSERT_EQUAL(1, result);
	// Vérification que le pointeur est bien stocké
	TEST_ASSERT_EQUAL_PTR(&gpu_profiler_system, post_proc.gpu_profiler);

	TEST_ASSERT_NOT_EQUAL(0, post_proc.scene_fbo);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.scene_color_tex);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.scene_depth_tex);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.screen_quad_vao);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.screen_quad_vbo);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.postprocess_shader);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.bloom_fx.fbo);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.bloom_fx.mips[0].texture);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.dof_fx.fbo);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.dof_fx.blur_tex);

	TEST_ASSERT_EQUAL(TestWidth, post_proc.width);
	TEST_ASSERT_EQUAL(TestHeight, post_proc.height);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_defaults(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	TEST_ASSERT_EQUAL(DEFAULT_ACTIVE_EFFECTS, post_proc.active_effects);
	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon, DEFAULT_EXPOSURE,
	                         post_proc.exposure.exposure);
	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon, DEFAULT_VIGNETTE_INTENSITY,
	                         post_proc.vignette.intensity);
	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon, DefaultNeutral,
	                         post_proc.color_grading.saturation);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_toggle_effects(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	TEST_ASSERT_FALSE(postprocess_is_enabled(&post_proc, POSTFX_VIGNETTE));

	postprocess_enable(&post_proc, POSTFX_VIGNETTE);
	TEST_ASSERT_TRUE(postprocess_is_enabled(&post_proc, POSTFX_VIGNETTE));

	postprocess_toggle(&post_proc, POSTFX_VIGNETTE);
	TEST_ASSERT_FALSE(postprocess_is_enabled(&post_proc, POSTFX_VIGNETTE));

	postprocess_toggle(&post_proc, POSTFX_VIGNETTE);
	TEST_ASSERT_TRUE(postprocess_is_enabled(&post_proc, POSTFX_VIGNETTE));

	postprocess_cleanup(&post_proc);
}

void test_postprocess_apply_preset(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	postprocess_apply_preset(&post_proc, &PRESET_VINTAGE);

	TEST_ASSERT_EQUAL(PRESET_VINTAGE.active_effects,
	                  post_proc.active_effects);
	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon, PRESET_VINTAGE.vignette.intensity,
	                         post_proc.vignette.intensity);
	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon, PRESET_VINTAGE.grain.intensity,
	                         post_proc.grain.intensity);
	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon, PRESET_VINTAGE.exposure.exposure,
	                         post_proc.exposure.exposure);
	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon,
	                         PRESET_VINTAGE.chrom_abbr.strength,
	                         post_proc.chrom_abbr.strength);

	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon,
	                         PRESET_VINTAGE.color_grading.contrast,
	                         post_proc.color_grading.contrast);
	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon, PRESET_VINTAGE.bloom.intensity,
	                         post_proc.bloom.intensity);
	TEST_ASSERT_FLOAT_WITHIN(TestEpsilon, PRESET_VINTAGE.dof.focal_distance,
	                         post_proc.dof.focal_distance);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_resize(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	postprocess_resize(&post_proc, NewDimension, NewDimension);

	TEST_ASSERT_EQUAL(NewDimension, post_proc.width);
	TEST_ASSERT_EQUAL(NewDimension, post_proc.height);

	TEST_ASSERT_TRUE(glIsFramebuffer(post_proc.scene_fbo));
	TEST_ASSERT_TRUE(glIsTexture(post_proc.scene_color_tex));

	glBindTexture(GL_TEXTURE_2D, post_proc.scene_color_tex);
	int tex_w = GL_INVALID;
	int tex_h = GL_INVALID;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, TEX_LEVEL_0, GL_TEXTURE_WIDTH,
	                         &tex_w);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, TEX_LEVEL_0, GL_TEXTURE_HEIGHT,
	                         &tex_h);
	TEST_ASSERT_EQUAL(NewDimension, tex_w);
	TEST_ASSERT_EQUAL(NewDimension, tex_h);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_cleanup(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	GLuint fbo_id = post_proc.scene_fbo;
	GLuint tex_id = post_proc.scene_color_tex;

	postprocess_cleanup(&post_proc);

	TEST_ASSERT_FALSE(glIsFramebuffer(fbo_id));
	TEST_ASSERT_FALSE(glIsTexture(tex_id));
	TEST_ASSERT_EQUAL(GL_INVALID, post_proc.scene_fbo);
	TEST_ASSERT_NULL(post_proc.postprocess_shader);
}

void test_postprocess_optimization_switch(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	TEST_ASSERT_TRUE(post_proc.is_optimized);
	GLuint original_program = post_proc.postprocess_shader->program;

	postprocess_use_dynamic(&post_proc);
	TEST_ASSERT_FALSE(post_proc.is_optimized);
	TEST_ASSERT_NOT_EQUAL(original_program,
	                      post_proc.postprocess_shader->program);

	unsigned int opt_flags = (unsigned int)(POSTFX_VIGNETTE | POSTFX_GRAIN);
	postprocess_compile_optimized(&post_proc, opt_flags);
	TEST_ASSERT_TRUE(post_proc.is_optimized);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_optimized_preset_switch(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	postprocess_compile_optimized(&post_proc, post_proc.active_effects);
	TEST_ASSERT_TRUE(post_proc.is_optimized);
	GLuint first_program = post_proc.postprocess_shader->program;

	postprocess_apply_preset(&post_proc, &PRESET_CINEMATIC);

	TEST_ASSERT_TRUE(post_proc.is_optimized);
	TEST_ASSERT_NOT_EQUAL(first_program,
	                      post_proc.postprocess_shader->program);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_recompilation_benchmark(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	unsigned int flags_a = (unsigned int)(POSTFX_VIGNETTE | POSTFX_GRAIN);
	unsigned int flags_b =
	    (unsigned int)(POSTFX_VIGNETTE | POSTFX_GRAIN | POSTFX_BLOOM);

	clock_t start = clock();
	for (int i = 0; i < LOOP_COUNT_20; ++i) {
		postprocess_compile_optimized(&post_proc, flags_a);
		postprocess_compile_optimized(&post_proc, flags_b);
	}
	clock_t end = clock();
	double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

	printf("Benchmark Time: %f seconds\n", cpu_time_used);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_cache_overflow_benchmark(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	for (unsigned int i = 0; i < LOOP_COUNT_32; ++i) {
		postprocess_compile_optimized(&post_proc, i);
	}

	clock_t start = clock();
	for (int i = 0; i < LOOP_COUNT_10; ++i) {
		postprocess_compile_optimized(&post_proc, 32);
		postprocess_compile_optimized(&post_proc, 33);
	}
	clock_t end = clock();
	double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

	printf("Overflow Benchmark Time: %f seconds\n", cpu_time_used);
	TEST_ASSERT_LESS_THAN_FLOAT(TIME_THRESHOLD, (float)cpu_time_used);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_large_working_set(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	for (unsigned int i = 0; i < LOOP_COUNT_40; ++i) {
		postprocess_compile_optimized(&post_proc, i);
	}

	clock_t start = clock();
	for (unsigned int i = 0; i < LOOP_COUNT_40; ++i) {
		postprocess_compile_optimized(&post_proc, i);
	}
	clock_t end = clock();
	double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

	printf("Large Working Set Time: %f seconds\n", cpu_time_used);

	TEST_ASSERT_LESS_THAN_FLOAT(TIME_THRESHOLD, (float)cpu_time_used);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_render_pipeline(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	/* Enable all effects to cover more branches in postprocess_end */
	post_proc.active_effects = 0xFFFFFFFFU;
	post_proc.ubo_dirty = true;

	postprocess_begin(&post_proc);
	/* Simulate drawing */
	postprocess_end(&post_proc);

	/* Cover many setters and state changes (correcting signatures) */
	postprocess_set_exposure(&post_proc, 1.5F);
	postprocess_set_tonemapper(&post_proc, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F);
	postprocess_set_vignette(&post_proc, 0.5F, 0.5F, 1.0F);
	postprocess_set_bloom(&post_proc, 1.0F, 0.8F, 0.5F);
	postprocess_set_dof(&post_proc, 10.0F, 5.0F, 1.0F);
	postprocess_set_grain(&post_proc, 0.1F);
	postprocess_set_chrom_abbr(&post_proc, 0.05F);
	postprocess_set_color_grading(&post_proc, 1.1F, 1.1F, 1.0F, 1.0F, 0.0F);
	postprocess_set_white_balance(&post_proc, 6500.00F, 0.1F);
	postprocess_set_fxaa(&post_proc, 0.75F, 0.125F, 0.063F);
	postprocess_set_banding(&post_proc, BANDING_MODE_LINEAR, 256.0F);
	postprocess_set_auto_exposure(&post_proc, 0.1F, 10.0F, 1.0F, 1.0F,
	                              1.0F);

	/* Check some internal state */
	TEST_ASSERT_TRUE(post_proc.ubo_dirty);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_banding_variants(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, &gpu_profiler_system, SmallTestWidth,
	                 SmallTestHeight);

	postprocess_set_banding_dither(&post_proc, 0.5F);
	postprocess_set_banding_perceptual(&post_proc, 2.2F);
	postprocess_set_banding_channels(&post_proc, 128.0F, 128.0F, 128.0F);

	postprocess_cleanup(&post_proc);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_postprocess_init_creates_resources);
	RUN_TEST(test_postprocess_defaults);
	RUN_TEST(test_postprocess_toggle_effects);
	RUN_TEST(test_postprocess_apply_preset);
	RUN_TEST(test_postprocess_resize);
	RUN_TEST(test_postprocess_optimization_switch);
	RUN_TEST(test_postprocess_optimized_preset_switch);
	RUN_TEST(test_postprocess_recompilation_benchmark);
	RUN_TEST(test_postprocess_cache_overflow_benchmark);
	RUN_TEST(test_postprocess_large_working_set);
	RUN_TEST(test_postprocess_render_pipeline);
	RUN_TEST(test_postprocess_banding_variants);
	RUN_TEST(test_postprocess_cleanup);
	return UNITY_END();
}
