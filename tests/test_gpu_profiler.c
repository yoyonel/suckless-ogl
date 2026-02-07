#include <glad/glad.h>

#include "gpu_profiler.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Global state
static GLFWwindow* test_window = NULL;

static const int TEST_WINDOW_WIDTH = 640;
static const int TEST_WINDOW_HEIGHT = 480;
static const uint32_t COLOR_RED = 0xFF0000;
static const uint32_t COLOR_GREEN = 0x00FF00;
static const int TEST_LOOP_COUNT =
    10;  // Augmenté pour garantir le cycle Ping-Pong
static const float CLEAR_COLOR_VAL = 0.1F;

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

	test_window = glfwCreateWindow(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT,
	                               "Test Window", NULL, NULL);
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

/**
 * @brief test_gpu_profiler_init
 * Vérifie l'état initial propre (nettoyage des leaks potentiels).
 */
void test_gpu_profiler_init(void)
{
	GPUProfiler profiler;
	gpu_profiler_init(&profiler);

	TEST_ASSERT_EQUAL(0, profiler.stage_count);
	TEST_ASSERT_EQUAL(0, profiler.write_index);

	// Le read_index est initialisé à 1 pour le Ping-Pong
	TEST_ASSERT_EQUAL(1, profiler.read_index);

	gpu_profiler_cleanup(&profiler);
}

/**
 * @brief test_gpu_profiler_double_buffering_swap
 * Vérifie la logique de swap Ping-Pong des buffers de requêtes.
 */
void test_gpu_profiler_double_buffering_swap(void)
{
	GPUProfiler profiler;
	gpu_profiler_init(&profiler);

	// Initial State: Write 0, Read 1
	TEST_ASSERT_EQUAL(0, profiler.write_index);
	TEST_ASSERT_EQUAL(1, profiler.read_index);

	// Frame 1 -> Frame 2
	gpu_profiler_begin_frame(&profiler, 0);
	TEST_ASSERT_EQUAL(1, profiler.write_index);
	TEST_ASSERT_EQUAL(0, profiler.read_index);

	// Frame 2 -> Frame 3
	gpu_profiler_begin_frame(&profiler, 0);
	TEST_ASSERT_EQUAL(0, profiler.write_index);
	TEST_ASSERT_EQUAL(1, profiler.read_index);

	gpu_profiler_cleanup(&profiler);
}

/**
 * @brief test_gpu_profiler_stage_registration
 * Vérifie l'enregistrement des étapes sans appeler OpenGL (juste la structure).
 */
void test_gpu_profiler_stage_registration(void)
{
	GPUProfiler profiler;
	gpu_profiler_init(&profiler);

	// Start Stage A
	gpu_profiler_start_stage(&profiler, "Stage A", COLOR_RED);
	TEST_ASSERT_EQUAL(1, profiler.stage_count);
	TEST_ASSERT_EQUAL_STRING("Stage A", profiler.stages[0].name);
	TEST_ASSERT_EQUAL(1,
	                  metric_stack_get_depth(
	                      &profiler.hierarchy_stack)); /* Nesting active */

	// End Stage A
	gpu_profiler_end_stage(&profiler);
	TEST_ASSERT_EQUAL(
	    0, metric_stack_get_depth(
	           &profiler.hierarchy_stack)); /* Nesting finished */

	// Start Stage B
	gpu_profiler_start_stage(&profiler, "Stage B", COLOR_GREEN);
	TEST_ASSERT_EQUAL(2, profiler.stage_count);
	TEST_ASSERT_EQUAL_STRING("Stage B", profiler.stages[1].name);
	gpu_profiler_end_stage(&profiler);

	gpu_profiler_cleanup(&profiler);
}

/**
 * @brief test_gpu_profiler_result_retrieval
 * Test d'intégration complet avec le GPU.
 * Utilise glFinish() DANS LE TEST UNIQUEMENT pour garantir que les données
 * sont disponibles pour les assertions, simulant un cycle normal de frame.
 */
void test_gpu_profiler_result_retrieval(void)
{
	GPUProfiler profiler;
	gpu_profiler_init(&profiler);

	// On boucle suffisamment pour amorcer le buffer circulaire (Ping-Pong)
	// Frame 0: Write Buffer 0
	// Frame 1: Read Buffer 1 (Empty), Write Buffer 1
	// Frame 2: Read Buffer 0 (Has Data), Write Buffer 0
	for (int i = 0; i < TEST_LOOP_COUNT; ++i) {
		// 1. Lit les résultats de la frame précédente (si dispo)
		gpu_profiler_begin_frame(&profiler, i);

		// 2. Enregistre une nouvelle frame
		gpu_profiler_start_stage(&profiler, "Render", COLOR_RED);

		glClearColor(CLEAR_COLOR_VAL, CLEAR_COLOR_VAL, CLEAR_COLOR_VAL,
		             1.0F);
		glClear(GL_COLOR_BUFFER_BIT);

		gpu_profiler_end_stage(&profiler);

		// 3. FORCE la synchronisation pour ce test unitaire.
		// Dans l'app réelle, on n'utilise PAS glFinish, on attend la
		// frame suivante. Ici, on veut garantir que begin_frame() aura
		// 'available=TRUE' à la prochaine itération.
		glFinish();
	}

	// Appel final pour lire les derniers résultats
	gpu_profiler_begin_frame(&profiler, TEST_LOOP_COUNT);

	// Vérifications
	// Le sampler doit avoir accumulé des échantillons
	TEST_ASSERT_GREATER_THAN(0, profiler.stages[0].duration_sampler.count);

	// La durée doit être positive ou nulle (0.0 est possible sur CI
	// rapide/software)
	TEST_ASSERT_TRUE(profiler.stages[0].duration_ms >= 0.0F);

	gpu_profiler_cleanup(&profiler);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gpu_profiler_init);
	RUN_TEST(test_gpu_profiler_double_buffering_swap);
	RUN_TEST(test_gpu_profiler_stage_registration);
	RUN_TEST(test_gpu_profiler_result_retrieval);
	return UNITY_END();
}
