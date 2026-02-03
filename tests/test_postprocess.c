#include <glad/glad.h>

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

static GLFWwindow* test_window = NULL;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	// Hidden window for headless testing
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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
}

void tearDown(void)
{
	if (test_window) {
		glfwDestroyWindow(test_window);
	}
	glfwTerminate();
}

void test_postprocess_init_creates_resources(void)
{
	PostProcess post_proc = {0};
	int result = postprocess_init(&post_proc, TestWidth, TestHeight);

	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.scene_fbo);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.scene_color_tex);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.scene_depth_tex);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.screen_quad_vao);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.screen_quad_vbo);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.postprocess_shader);
	/* Bloom resources */
	TEST_ASSERT_NOT_EQUAL(0, post_proc.bloom_fx.fbo);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.bloom_fx.mips[0].texture);
	/* DoF resources */
	TEST_ASSERT_NOT_EQUAL(0, post_proc.dof_fx.fbo);
	TEST_ASSERT_NOT_EQUAL(0, post_proc.dof_fx.blur_tex);

	TEST_ASSERT_EQUAL(TestWidth, post_proc.width);
	TEST_ASSERT_EQUAL(TestHeight, post_proc.height);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_defaults(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, SmallTestWidth, SmallTestHeight);

	// Updated to check for default effects instead of 0
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
	postprocess_init(&post_proc, SmallTestWidth, SmallTestHeight);

	// Initial state: Disabled (not in DEFAULT_ACTIVE_EFFECTS)
	TEST_ASSERT_FALSE(postprocess_is_enabled(&post_proc, POSTFX_VIGNETTE));

	// Enable
	postprocess_enable(&post_proc, POSTFX_VIGNETTE);
	TEST_ASSERT_TRUE(postprocess_is_enabled(&post_proc, POSTFX_VIGNETTE));

	// Toggle (Disable)
	postprocess_toggle(&post_proc, POSTFX_VIGNETTE);
	TEST_ASSERT_FALSE(postprocess_is_enabled(&post_proc, POSTFX_VIGNETTE));

	// Toggle (Enable)
	postprocess_toggle(&post_proc, POSTFX_VIGNETTE);
	TEST_ASSERT_TRUE(postprocess_is_enabled(&post_proc, POSTFX_VIGNETTE));

	postprocess_cleanup(&post_proc);
}

void test_postprocess_apply_preset(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, SmallTestWidth, SmallTestHeight);

	// Apply Vintage preset
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

	// Let's check color grading
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
	postprocess_init(&post_proc, SmallTestWidth, SmallTestHeight);

	postprocess_resize(&post_proc, NewDimension, NewDimension);

	TEST_ASSERT_EQUAL(NewDimension, post_proc.width);
	TEST_ASSERT_EQUAL(NewDimension, post_proc.height);

	// Validate that resources were recreated
	TEST_ASSERT_TRUE(glIsFramebuffer(post_proc.scene_fbo));
	TEST_ASSERT_TRUE(glIsTexture(post_proc.scene_color_tex));

	// Check dimensions of the texture
	glBindTexture(GL_TEXTURE_2D, post_proc.scene_color_tex);
	int tex_w = 0;
	int tex_h = 0;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_w);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_h);
	TEST_ASSERT_EQUAL(NewDimension, tex_w);
	TEST_ASSERT_EQUAL(NewDimension, tex_h);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_cleanup(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, SmallTestWidth, SmallTestHeight);

	GLuint fbo_id = post_proc.scene_fbo;
	GLuint tex_id = post_proc.scene_color_tex;

	postprocess_cleanup(&post_proc);

	TEST_ASSERT_FALSE(glIsFramebuffer(fbo_id));
	TEST_ASSERT_FALSE(glIsTexture(tex_id));
	TEST_ASSERT_EQUAL(0, post_proc.scene_fbo);
	TEST_ASSERT_NULL(post_proc.postprocess_shader);
}

void test_postprocess_optimization_switch(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, SmallTestWidth, SmallTestHeight);

	// Default should be dynamic
	TEST_ASSERT_FALSE(post_proc.is_optimized);
	GLuint original_program = post_proc.postprocess_shader->program;

	// Switch to optimized
	unsigned int opt_flags = (unsigned int)(POSTFX_VIGNETTE | POSTFX_GRAIN);
	postprocess_compile_optimized(&post_proc, opt_flags);
	TEST_ASSERT_TRUE(post_proc.is_optimized);
	TEST_ASSERT_NOT_EQUAL(original_program,
	                      post_proc.postprocess_shader->program);

	// Switch back to dynamic
	GLuint optimized_program = post_proc.postprocess_shader->program;
	postprocess_use_dynamic(&post_proc);
	TEST_ASSERT_FALSE(post_proc.is_optimized);
	TEST_ASSERT_NOT_EQUAL(optimized_program,
	                      post_proc.postprocess_shader->program);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_optimized_preset_switch(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, SmallTestWidth, SmallTestHeight);

	// Enable optimization
	postprocess_compile_optimized(&post_proc, post_proc.active_effects);
	TEST_ASSERT_TRUE(post_proc.is_optimized);
	GLuint first_program = post_proc.postprocess_shader->program;

	// Apply a different preset
	postprocess_apply_preset(&post_proc, &PRESET_CINEMATIC);

	// Should have recompiled
	TEST_ASSERT_TRUE(post_proc.is_optimized);
	TEST_ASSERT_NOT_EQUAL(first_program,
	                      post_proc.postprocess_shader->program);

	postprocess_cleanup(&post_proc);
}

void test_postprocess_recompilation_benchmark(void)
{
	PostProcess post_proc = {0};
	postprocess_init(&post_proc, SmallTestWidth, SmallTestHeight);

	unsigned int flags_a = (unsigned int)(POSTFX_VIGNETTE | POSTFX_GRAIN);
	unsigned int flags_b =
	    (unsigned int)(POSTFX_VIGNETTE | POSTFX_GRAIN | POSTFX_BLOOM);

	clock_t start = clock();
	for (int i = 0; i < 20; ++i) {
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
	postprocess_init(&post_proc, SmallTestWidth, SmallTestHeight);

	// Fill cache with 32 variants
	for (unsigned int i = 0; i < 32; ++i) {
		postprocess_compile_optimized(&post_proc, i);
	}

	// Now cycle between variant 32 (uncached) and 33 (uncached)
	// With LRU, they should replace each other but stay cached if we
	// alternate Wait, if we alternate 32 and 33: 32 replaces LRU. 33
	// replaces LRU. 32 found (LRU). 33 found (LRU).

	clock_t start = clock();
	for (int i = 0; i < 10; ++i) {
		postprocess_compile_optimized(&post_proc, 32);
		postprocess_compile_optimized(&post_proc, 33);
	}
	clock_t end = clock();
	double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

	printf("Overflow Benchmark Time: %f seconds\n", cpu_time_used);
	// We expect this to be fast (cached hits after first 2), not slow
	// (recompilation every time) Threshold: 0.05s (50ms). Uncached was
	// ~110ms. Cached was ~10ms.
	// This proves that the LRU policy is working by keeping the 32 most
	// recent items.
	TEST_ASSERT_LESS_THAN_FLOAT(0.05f, (float)cpu_time_used);

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
	RUN_TEST(test_postprocess_cleanup);
	return UNITY_END();
}
