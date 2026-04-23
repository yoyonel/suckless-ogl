/**
 * @file test_benchmark_buffer_upload.c
 * @brief Benchmark: NBody update + draw cycle using real application API.
 *
 * Measures the wall-clock cost of the NBody per-frame update pipeline
 * (physics → instance build → VBO upload → draw) and the trail renderer
 * (record → ribbon build → VBO upload → draw) using the actual application
 * functions, not synthetic GL call reproductions.
 *
 * This ensures the benchmark stays coupled to the real code paths and
 * reflects any future changes to the rendering pipeline.
 *
 * @see docs/nbody_buffer_optimization.md
 */

#include "gl_common.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "nbody.h"
#include "trail_renderer.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static GLFWwindow* test_window = NULL;

/** Number of benchmark frames (draw→update cycles). */
static const int FRAMES = 300;

/** Number of warmup frames to prime the driver and fill trail rings. */
static const int WARMUP_FRAMES = 60;

/** Simulated delta_time at 60 FPS. */
static const float SIMULATED_DT = 1.0F / 60.0F;

/** Frame budget at 60 FPS in microseconds. */
static const double FRAME_BUDGET_US = 16666.7;

/** Icosphere subdivision level (matches INITIAL_SUBDIVISIONS = 3). */
static const int BENCH_SUBDIVISIONS = 3;

/* Microsecond-precision timer using CLOCK_MONOTONIC. */
static double get_time_us(void)
{
	struct timespec ts;
	(void)clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

void setUp(void)
{
	if (!glfwInit()) {
		return;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	test_window = glfwCreateWindow(1, 1, "Benchmark", NULL, NULL);
	if (!test_window) {
		glfwTerminate();
		return;
	}

	glfwMakeContextCurrent(test_window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
}

void tearDown(void)
{
	if (test_window) {
		glfwDestroyWindow(test_window);
		test_window = NULL;
	}
	glfwTerminate();
}

/**
 * @brief Benchmark the full NBody instance update pipeline.
 *
 * Exercises the real code path:
 *   nbody_step() → nbody_write_instances() → instanced_group_update()
 *                → instanced_group_draw()
 *
 * Measures the update+upload portion (not the draw call itself)
 * which is where the CPU↔GPU sync stall lives.
 */
void test_benchmark_nbody_instance_update(void)
{
	if (!test_window) {
		TEST_IGNORE_MESSAGE("OpenGL context not available");
	}

	/* --- Setup: real NBody simulation --- */
	NBodySim sim;
	nbody_init_preset(&sim);
	int count = nbody_get_count(&sim);

	/* --- Setup: real InstancedGroup --- */
	SphereInstance instances[NBODY_MAX_BODIES];
	nbody_write_instances(&sim, instances);

	InstancedGroup group;
	instanced_group_init(&group, instances, count);

	/* Create a minimal mesh (icosphere) for realistic draw calls */
	IcosphereGeometry geom = {0};
	icosphere_generate(&geom, BENCH_SUBDIVISIONS);

	GLuint vbo = 0;
	GLuint nbo = 0;
	GLuint ebo = 0;
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &nbo);
	glGenBuffers(1, &ebo);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(geom.vertices.size * sizeof(float)),
	             geom.vertices.data, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, nbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(geom.normals.size * sizeof(float)),
	             geom.normals.data, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
	             (GLsizeiptr)(geom.indices.size * sizeof(unsigned int)),
	             geom.indices.data, GL_STATIC_DRAW);

	instanced_group_bind_mesh(&group, vbo, nbo, ebo);

	/* --- Warmup: fill the GPU pipeline --- */
	for (int i = 0; i < WARMUP_FRAMES; i++) {
		nbody_step(&sim, SIMULATED_DT);
		nbody_write_instances(&sim, instances);
		instanced_group_update(&group, instances, count);
		instanced_group_draw(&group, geom.indices.size);
		glFinish();
	}

	/* --- Benchmark: draw → update → measure --- */
	double total_update_us = 0.0;
	double min_us = 1e9;
	double max_us = 0.0;

	for (int i = 0; i < FRAMES; i++) {
		/* Draw first (puts VBO in-flight on GPU) */
		instanced_group_draw(&group, geom.indices.size);

		/* Physics step — real O(N²) computation */
		nbody_step(&sim, SIMULATED_DT);

		/* Measure: instance build + VBO upload (the stall zone) */
		double t0 = get_time_us();
		nbody_write_instances(&sim, instances);
		instanced_group_update(&group, instances, count);
		glFinish();
		double t1 = get_time_us();

		double elapsed = t1 - t0;
		total_update_us += elapsed;
		if (elapsed < min_us) {
			min_us = elapsed;
		}
		if (elapsed > max_us) {
			max_us = elapsed;
		}
	}

	double avg_us = total_update_us / FRAMES;
	double pct_budget = (avg_us / FRAME_BUDGET_US) * 100.0;

	printf("\n=== NBody Instance Update Benchmark ===\n");
	printf("Bodies: %d  |  Mesh: %zu indices (subdiv %d)\n", count,
	       geom.indices.size, BENCH_SUBDIVISIONS);
	printf("Frames: %d (warmup: %d)\n", FRAMES, WARMUP_FRAMES);
	printf("Avg update+upload: %.1f us  (%.2f%% of 60fps budget)\n", avg_us,
	       pct_budget);
	printf("Min: %.1f us  |  Max: %.1f us\n", min_us, max_us);
	printf("Total update time: %.1f ms over %d frames\n",
	       total_update_us / 1000.0, FRAMES);

	/* Cleanup */
	instanced_group_cleanup(&group);
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &nbo);
	glDeleteBuffers(1, &ebo);
	icosphere_free(&geom);

	TEST_PASS();
}

/**
 * @brief Benchmark the full trail renderer pipeline.
 *
 * Exercises the real code path:
 *   trail_renderer_record() → trail_renderer_draw()
 *   (which internally does: ribbon build → orphan + upload → draw)
 *
 * Measures the entire draw call (build + upload + render) to capture
 * both the CPU-side ribbon construction and the GPU upload cost.
 */
void test_benchmark_trail_renderer(void)
{
	if (!test_window) {
		TEST_IGNORE_MESSAGE("OpenGL context not available");
	}

	/* --- Setup: real NBody simulation --- */
	NBodySim sim;
	nbody_init_preset(&sim);
	int count = nbody_get_count(&sim);

	/* --- Setup: real TrailRenderer --- */
	TrailRenderer trails;
	if (!trail_renderer_init(&trails, count)) {
		TEST_IGNORE_MESSAGE("Trail shader not available (headless)");
	}

	/* Set trail colors from body albedos (like scene_toggle_nbody) */
	for (int i = 0; i < count; i++) {
		trail_renderer_set_color(&trails, i, sim.bodies[i].albedo);
	}

	/* Fake camera matrices for trail_renderer_draw */
	mat4 view;
	mat4 proj;
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	vec3 cam_pos = {0.0F, 5.0F, 15.0F};

	/* --- Warmup: fill trail ring buffers --- */
	for (int i = 0; i < WARMUP_FRAMES; i++) {
		nbody_step(&sim, SIMULATED_DT);
		trail_renderer_record(&trails, &sim, SIMULATED_DT);
		trail_renderer_draw(&trails, view, proj, cam_pos);
		glFinish();
	}

	/* --- Benchmark: record + draw --- */
	double total_draw_us = 0.0;
	double min_us = 1e9;
	double max_us = 0.0;

	for (int i = 0; i < FRAMES; i++) {
		nbody_step(&sim, SIMULATED_DT);
		trail_renderer_record(&trails, &sim, SIMULATED_DT);

		/* Measure: ribbon build + upload + draw (the full pipeline) */
		double t0 = get_time_us();
		trail_renderer_draw(&trails, view, proj, cam_pos);
		glFinish();
		double t1 = get_time_us();

		double elapsed = t1 - t0;
		total_draw_us += elapsed;
		if (elapsed < min_us) {
			min_us = elapsed;
		}
		if (elapsed > max_us) {
			max_us = elapsed;
		}
	}

	double avg_us = total_draw_us / FRAMES;
	double pct_budget = (avg_us / FRAME_BUDGET_US) * 100.0;

	printf("\n=== Trail Renderer Benchmark ===\n");
	printf("Bodies: %d  |  Trail depth: %d samples\n", count,
	       TRAIL_MAX_POINTS);
	printf("Frames: %d (warmup: %d)\n", FRAMES, WARMUP_FRAMES);
	printf(
	    "Avg draw (build+upload+render): %.1f us  "
	    "(%.2f%% of 60fps budget)\n",
	    avg_us, pct_budget);
	printf("Min: %.1f us  |  Max: %.1f us\n", min_us, max_us);
	printf("Total draw time: %.1f ms over %d frames\n",
	       total_draw_us / 1000.0, FRAMES);

	/* Cleanup */
	trail_renderer_cleanup(&trails);

	TEST_PASS();
}

/**
 * @brief Verify that instanced_group_update preserves data integrity.
 *
 * Uses the real API: init → update → readback → verify.
 */
void test_instance_update_data_integrity(void)
{
	if (!test_window) {
		TEST_IGNORE_MESSAGE("OpenGL context not available");
	}

	/* Build instance data from the real simulation */
	NBodySim sim;
	nbody_init_preset(&sim);
	int count = nbody_get_count(&sim);

	SphereInstance instances[NBODY_MAX_BODIES];
	nbody_write_instances(&sim, instances);

	/* Init + update through the real API */
	InstancedGroup group;
	instanced_group_init(&group, instances, count);

	/* Step physics and update (exercises orphaning path) */
	nbody_step(&sim, SIMULATED_DT);
	nbody_write_instances(&sim, instances);
	instanced_group_update(&group, instances, count);

	/* Read back GPU buffer and verify */
	SphereInstance readback[NBODY_MAX_BODIES];
	(void)memset(readback, 0xFF, sizeof(readback));

	glBindBuffer(GL_ARRAY_BUFFER, group.instance_vbo);
	glGetBufferSubData(GL_ARRAY_BUFFER, 0,
	                   (GLsizeiptr)(count * sizeof(SphereInstance)),
	                   readback);

	for (int i = 0; i < count; i++) {
		TEST_ASSERT_FLOAT_WITHIN(1e-6F, instances[i].metallic,
		                         readback[i].metallic);
		TEST_ASSERT_FLOAT_WITHIN(1e-6F, instances[i].roughness,
		                         readback[i].roughness);
		TEST_ASSERT_FLOAT_WITHIN(1e-4F, instances[i].albedo[0],
		                         readback[i].albedo[0]);
	}

	instanced_group_cleanup(&group);
}

int main(void)
{
	/* Print GL renderer info before any test (requires a context) */
	if (!glfwInit()) {
		return 1;
	}
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* w = glfwCreateWindow(1, 1, "probe", NULL, NULL);
	if (w) {
		glfwMakeContextCurrent(w);
		gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		printf("GL Renderer: %s\n", glGetString(GL_RENDERER));
		printf("GL Version:  %s\n", glGetString(GL_VERSION));
		glfwDestroyWindow(w);
	}
	glfwTerminate();

	UNITY_BEGIN();
	RUN_TEST(test_benchmark_nbody_instance_update);
	RUN_TEST(test_benchmark_trail_renderer);
	RUN_TEST(test_instance_update_data_integrity);
	return UNITY_END();
}
