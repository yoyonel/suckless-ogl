#include "app.h"
#include "main.h"
#include "unity.h"
#include <GLFW/glfw3.h>

// Instance App partagée entre tous les tests
static App g_test_app;
static bool g_app_initialized = false;

void setUp(void)
{
	// Initialiser une seule fois pour tous les tests
	if (!g_app_initialized) {
		int result = app_init(&g_test_app, WINDOW_WIDTH, WINDOW_HEIGHT,
		                      "Integration Test");
		TEST_ASSERT_EQUAL_INT(1, result);
		g_app_initialized = true;
	}
}

void tearDown(void)
{
	// Ne rien faire ici, on cleanup dans main()
}

/**
 * Integration Test: Full lifecycle and single frame rendering validation
 */
void test_app_render_single_frame(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	// Get actual framebuffer dimensions
	int fb_width, fb_height;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);
	printf("Resolution: %dx%d\n", fb_width, fb_height);

	// Generate geometry and render
	icosphere_generate(&g_test_app.geometry, g_test_app.subdivisions);
	app_update_gpu_buffers(&g_test_app);
	app_render(&g_test_app);

	// Capture current frame pixels
	size_t pixel_data_size = fb_width * fb_height * 3;
	unsigned char* current_pixels = malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(current_pixels);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, fb_width, fb_height, GL_RGB, GL_UNSIGNED_BYTE,
	             current_pixels);

	// Load reference frame
	unsigned char* ref_pixels = malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(ref_pixels);

	FILE* fref = fopen("tests/ref_frame.raw", "rb");
	if (fref == NULL) {
		free(current_pixels);
		free(ref_pixels);
		TEST_FAIL_MESSAGE(
		    "Reference image tests/ref_frame.raw not found.");
	}

	size_t read_bytes = fread(ref_pixels, 1, pixel_data_size, fref);
	fclose(fref);
	TEST_ASSERT_EQUAL_UINT_MESSAGE(pixel_data_size, read_bytes,
	                               "Reference file size mismatch");

	// Compare with tolerance
	int diff_count = 0;
	const float pixel_tol = 5.0f;  // tolérance L2 en valeur 0-255

	for (size_t i = 0; i < pixel_data_size; i += 3) {
		float dr =
		    (float)current_pixels[i + 0] - (float)ref_pixels[i + 0];
		float dg =
		    (float)current_pixels[i + 1] - (float)ref_pixels[i + 1];
		float db =
		    (float)current_pixels[i + 2] - (float)ref_pixels[i + 2];
		float dist = sqrtf(dr * dr + dg * dg + db * db);
		if (dist > pixel_tol)
			diff_count++;
	}
	float diff_percentage =
	    (float)diff_count / (float)(fb_width * fb_height);

	// Always generate diff map and actual frame for CI visual reporting
	unsigned char* diff_map = malloc(pixel_data_size);
	for (size_t i = 0; i < pixel_data_size; i++) {
		int delta = abs((int)current_pixels[i] - (int)ref_pixels[i]);
		diff_map[i] = (delta > pixel_tol) ? 255 : 0;
	}

	// Save diagnostic files (always, for CI visual report)
	FILE* fdiff = fopen("tests/failed_diff_map.raw", "wb");
	if (fdiff) {
		fwrite(diff_map, 1, pixel_data_size, fdiff);
		fclose(fdiff);
	}

	FILE* fcur = fopen("tests/failed_frame_actual.raw", "wb");
	if (fcur) {
		fwrite(current_pixels, 1, pixel_data_size, fcur);
		fclose(fcur);
	}

	free(diff_map);

	if (diff_percentage > 0.00f) {
		printf(
		    "\n[VISUAL] Regression detected! Diff: %.2f%% (saved to "
		    "tests/failed_diff_map.raw)\n",
		    diff_percentage * 100.0f);
	}

	// Allow up to 2% difference for MSAA/driver noise
	TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.0f, diff_percentage);

	free(current_pixels);
	free(ref_pixels);
}

/**
 * Test camera initialization
 */
void test_app_camera_initialization(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	// Verify camera is properly initialized
	TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, g_test_app.camera.zoom);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_app_render_single_frame);
	RUN_TEST(test_app_camera_initialization);

	// Cleanup APRÈS tous les tests
	if (g_app_initialized) {
		app_cleanup(&g_test_app);
	}

	return UNITY_END();
}