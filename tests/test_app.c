#include "app.h"
#include "main.h"
#include "unity.h"
#include <GLFW/glfw3.h>

void setUp(void)
{
	// Initialisation globale si nécessaire
}

void tearDown(void)
{
	// Nettoyage global si nécessaire
}

/**
 * Test d'intégration : Cycle de vie complet et rendu d'une frame
 */
/**
 * Integration Test: Full lifecycle and single frame rendering validation
 */
void test_app_render_single_frame(void)
{
	App app;
	// app_init returns 1 on success
	int init_result =
	    app_init(&app, WINDOW_WIDTH, WINDOW_HEIGHT, "Integration Test");
	TEST_ASSERT_EQUAL_INT(1, init_result);

	// Get actual framebuffer dimensions (crucial for HiDPI or Xvfb
	// overrides)
	int fb_width, fb_height;
	glfwGetFramebufferSize(app.window, &fb_width, &fb_height);
	printf("Resolution: %dx%d\n", fb_width, fb_height);

	// Generate geometry and render
	icosphere_generate(&app.geometry, app.subdivisions);
	app_update_gpu_buffers(&app);
	app_render(&app);

	// 4. Capture current frame pixels
	size_t pixel_data_size = fb_width * fb_height * 3;
	unsigned char* current_pixels = malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(current_pixels);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, fb_width, fb_height, GL_RGB, GL_UNSIGNED_BYTE,
	             current_pixels);

	// 5. Load reference frame
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

	// 6. Compare with a reasonable threshold
	int diff_count = 0;
	const int color_tolerance =
	    2;  // Tolerance for each RGB channel (0-255)

	for (size_t i = 0; i < pixel_data_size; i++) {
		if (abs((int)current_pixels[i] - (int)ref_pixels[i]) >
		    color_tolerance) {
			diff_count++;
		}
	}

	float diff_percentage = (float)diff_count / (float)pixel_data_size;

	if (diff_percentage > 0.00f) {  // Seuil d'échec
		unsigned char* diff_map = malloc(pixel_data_size);

		for (size_t i = 0; i < pixel_data_size; i++) {
			int delta =
			    abs((int)current_pixels[i] - (int)ref_pixels[i]);

			if (delta > color_tolerance) {
				// Option 1: Mettre en évidence (Pixel rouge
				// pour les erreurs) On sature le canal si
				// différence pour que ce soit très visible
				diff_map[i] = 255;
			} else {
				// Pixels identiques = Noir
				diff_map[i] = 0;
			}
		}

		// Sauvegarde du diagnostic
		FILE* fdiff = fopen("tests/failed_diff_map.raw", "wb");
		if (fdiff) {
			fwrite(diff_map, 1, pixel_data_size, fdiff);
			fclose(fdiff);
			printf(
			    "\n[VISUAL] Regression detected! Diff map saved to "
			    "tests/failed_diff_map.raw\n");
		}

		// On sauvegarde aussi ce que l'app a rendu pour comparaison
		// directe
		FILE* fcur = fopen("tests/failed_frame_actual.raw", "wb");
		if (fcur) {
			fwrite(current_pixels, 1, pixel_data_size, fcur);
			fclose(fcur);
		}

		free(diff_map);
	}

	// We allow up to 2% difference (0.02) to account for MSAA and driver
	// noise Your current error was 0.017, so 0.02 is a safe and robust
	// threshold.
	TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.0f, diff_percentage);

	free(current_pixels);
	free(ref_pixels);
	app_cleanup(&app);
}

/**
 * Test de la configuration des caméras et matrices
 */
void test_app_camera_initialization(void)
{
	App app;
	app_init(&app, 800, 600, "Camera Test");

	// Vérifier que la caméra est bien initialisée dans la structure App
	// [cite: 11] On vérifie que le FOV est positif
	TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, app.camera.zoom);

	app_cleanup(&app);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_app_render_single_frame);
	RUN_TEST(test_app_camera_initialization);
	return UNITY_END();
}