#include <glad/glad.h>

#include "postprocess.h"
#include "postprocess_presets.h"
#include "unity.h"
#include <GLFW/glfw3.h>

static GLFWwindow* window = NULL;

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

	window = glfwCreateWindow(640, 480, "Test Window", NULL, NULL);
	if (!window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create GLFW window");
	}

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to initialize GLAD");
	}
}

void tearDown(void)
{
	if (window) {
		glfwDestroyWindow(window);
	}
	glfwTerminate();
}

void test_postprocess_init_creates_resources(void)
{
	PostProcess pp = {0};
	int result = postprocess_init(&pp, 640, 480, 4);

	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_NOT_EQUAL(0, pp.scene_fbo);
	TEST_ASSERT_NOT_EQUAL(0, pp.scene_color_tex);
	TEST_ASSERT_NOT_EQUAL(0, pp.scene_depth_tex);
	TEST_ASSERT_NOT_EQUAL(0, pp.screen_quad_vao);
	TEST_ASSERT_NOT_EQUAL(0, pp.screen_quad_vbo);
	TEST_ASSERT_NOT_EQUAL(0, pp.postprocess_shader);
	/* Bloom resources */
	TEST_ASSERT_NOT_EQUAL(0, pp.bloom_fx.fbo);
	TEST_ASSERT_NOT_EQUAL(0, pp.bloom_fx.mips[0].texture);
	/* DoF resources */
	TEST_ASSERT_NOT_EQUAL(0, pp.dof_fx.fbo);
	TEST_ASSERT_NOT_EQUAL(0, pp.dof_fx.blur_tex);

	TEST_ASSERT_EQUAL(640, pp.width);
	TEST_ASSERT_EQUAL(480, pp.height);

	postprocess_cleanup(&pp);
}

void test_postprocess_defaults(void)
{
	PostProcess pp = {0};
	postprocess_init(&pp, 100, 100, 4);

	TEST_ASSERT_EQUAL(POSTFX_FXAA, pp.active_effects);
	TEST_ASSERT_FLOAT_WITHIN(1e-5, DEFAULT_EXPOSURE, pp.exposure.exposure);
	TEST_ASSERT_FLOAT_WITHIN(1e-5, DEFAULT_VIGNETTE_INTENSITY,
	                         pp.vignette.intensity);
	TEST_ASSERT_FLOAT_WITHIN(1e-5, 1.0f, pp.color_grading.saturation);

	postprocess_cleanup(&pp);
}

void test_postprocess_toggle_effects(void)
{
	PostProcess pp = {0};
	postprocess_init(&pp, 100, 100, 4);

	// Initial state: 0
	TEST_ASSERT_FALSE(postprocess_is_enabled(&pp, POSTFX_VIGNETTE));

	// Enable
	postprocess_enable(&pp, POSTFX_VIGNETTE);
	TEST_ASSERT_TRUE(postprocess_is_enabled(&pp, POSTFX_VIGNETTE));

	// Toggle (Disable)
	postprocess_toggle(&pp, POSTFX_VIGNETTE);
	TEST_ASSERT_FALSE(postprocess_is_enabled(&pp, POSTFX_VIGNETTE));

	// Toggle (Enable)
	postprocess_toggle(&pp, POSTFX_VIGNETTE);
	TEST_ASSERT_TRUE(postprocess_is_enabled(&pp, POSTFX_VIGNETTE));

	// Disable
	postprocess_disable(&pp, POSTFX_VIGNETTE);
	TEST_ASSERT_FALSE(postprocess_is_enabled(&pp, POSTFX_VIGNETTE));

	postprocess_cleanup(&pp);
}

void test_postprocess_apply_preset(void)
{
	PostProcess pp = {0};
	postprocess_init(&pp, 100, 100, 4);

	// Apply Vintage preset
	postprocess_apply_preset(&pp, &PRESET_VINTAGE);

	TEST_ASSERT_EQUAL(PRESET_VINTAGE.active_effects, pp.active_effects);
	TEST_ASSERT_FLOAT_WITHIN(1e-5, PRESET_VINTAGE.vignette.intensity,
	                         pp.vignette.intensity);
	TEST_ASSERT_FLOAT_WITHIN(1e-5, PRESET_VINTAGE.grain.intensity,
	                         pp.grain.intensity);
	TEST_ASSERT_FLOAT_WITHIN(1e-5, PRESET_VINTAGE.exposure.exposure,
	                         pp.exposure.exposure);
	TEST_ASSERT_FLOAT_WITHIN(1e-5, PRESET_VINTAGE.chrom_abbr.strength,
	                         pp.chrom_abbr.strength);

	// Initial is 0.0, Vintage is 0.0 or something else? Let's check color
	// grading
	TEST_ASSERT_FLOAT_WITHIN(1e-5, PRESET_VINTAGE.color_grading.contrast,
	                         pp.color_grading.contrast);
	TEST_ASSERT_FLOAT_WITHIN(1e-5, PRESET_VINTAGE.bloom.intensity,
	                         pp.bloom.intensity);
	TEST_ASSERT_FLOAT_WITHIN(1e-5, PRESET_VINTAGE.dof.focal_distance,
	                         pp.dof.focal_distance);

	postprocess_cleanup(&pp);
}

void test_postprocess_resize(void)
{
	PostProcess pp = {0};
	postprocess_init(&pp, 100, 100, 4);

	GLuint old_fbo = pp.scene_fbo;
	GLuint old_tex = pp.scene_color_tex;

	postprocess_resize(&pp, 200, 200);

	TEST_ASSERT_EQUAL(200, pp.width);
	TEST_ASSERT_EQUAL(200, pp.height);

	// Validate that resources were recreated (IDs might change or be
	// reused, but basic check is valid) Actually, glGenFramebuffers might
	// return the same ID if the old one was deleted. Instead check that FBO
	// is still complete and valid
	TEST_ASSERT_TRUE(glIsFramebuffer(pp.scene_fbo));
	TEST_ASSERT_TRUE(glIsTexture(pp.scene_color_tex));

	// Check dimensions of the texture
	glBindTexture(GL_TEXTURE_2D, pp.scene_color_tex);
	int w, h;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
	TEST_ASSERT_EQUAL(200, w);
	TEST_ASSERT_EQUAL(200, h);

	postprocess_cleanup(&pp);
}

void test_postprocess_cleanup(void)
{
	PostProcess pp = {0};
	postprocess_init(&pp, 100, 100, 4);

	GLuint fbo = pp.scene_fbo;
	GLuint tex = pp.scene_color_tex;

	postprocess_cleanup(&pp);

	TEST_ASSERT_FALSE(glIsFramebuffer(fbo));
	TEST_ASSERT_FALSE(glIsTexture(tex));
	TEST_ASSERT_EQUAL(0, pp.scene_fbo);
	TEST_ASSERT_NULL(pp.postprocess_shader);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_postprocess_init_creates_resources);
	RUN_TEST(test_postprocess_defaults);
	RUN_TEST(test_postprocess_toggle_effects);
	RUN_TEST(test_postprocess_apply_preset);
	RUN_TEST(test_postprocess_resize);
	RUN_TEST(test_postprocess_cleanup);
	return UNITY_END();
}
