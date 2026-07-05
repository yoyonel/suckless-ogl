#include "mock_gl_standalone.h"
#include "scene.h"
#include "scene_gpu_resources.h"
#include "scene_shaders.h"
#include "scene_simulation.h"
#include "scene_visuals.h"
#include "shader.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Call counters for stubs --- */
static int g_icosphere_free_calls = 0;
static int g_skybox_cleanup_calls = 0;
static int g_instanced_group_cleanup_calls = 0;
static int g_billboard_renderer_cleanup_calls = 0;
static int g_trail_renderer_cleanup_calls = 0;
static int g_shockwave_renderer_cleanup_calls = 0;
static int g_material_free_lib_calls = 0;
static int g_ibl_coordinator_cleanup_calls = 0;
static int g_light_probe_grid_cleanup_calls = 0;
static int g_shader_destroy_calls = 0;
static int g_platform_aligned_free_calls = 0;

#ifdef USE_SSBO_RENDERING
static int g_ssbo_group_cleanup_calls = 0;
#endif

/* --- Helper to reset all stub counters --- */
static void reset_stub_calls(void)
{
	g_icosphere_free_calls = 0;
	g_skybox_cleanup_calls = 0;
	g_instanced_group_cleanup_calls = 0;
	g_billboard_renderer_cleanup_calls = 0;
	g_trail_renderer_cleanup_calls = 0;
	g_shockwave_renderer_cleanup_calls = 0;
	g_material_free_lib_calls = 0;
	g_ibl_coordinator_cleanup_calls = 0;
	g_light_probe_grid_cleanup_calls = 0;
	g_shader_destroy_calls = 0;
	g_platform_aligned_free_calls = 0;
#ifdef USE_SSBO_RENDERING
	g_ssbo_group_cleanup_calls = 0;
#endif
}

/* --- Stubs for scene dependencies --- */
void icosphere_free(IcosphereGeometry* geometry)
{
	(void)geometry;
	g_icosphere_free_calls++;
}

void skybox_cleanup(Skybox* skybox)
{
	(void)skybox;
	g_skybox_cleanup_calls++;
}

void instanced_group_cleanup(InstancedGroup* group)
{
	(void)group;
	g_instanced_group_cleanup_calls++;
}

void billboard_renderer_cleanup(BillboardRenderer* renderer)
{
	(void)renderer;
	g_billboard_renderer_cleanup_calls++;
}

void trail_renderer_cleanup(TrailRenderer* renderer)
{
	(void)renderer;
	g_trail_renderer_cleanup_calls++;
}

void shockwave_renderer_cleanup(ShockwaveRenderer* renderer)
{
	(void)renderer;
	g_shockwave_renderer_cleanup_calls++;
}

#ifdef USE_SSBO_RENDERING
void ssbo_group_cleanup(SSBOGroup* group)
{
	(void)group;
	g_ssbo_group_cleanup_calls++;
}
#endif

void material_free_lib(MaterialLib* lib)
{
	(void)lib;
	g_material_free_lib_calls++;
}

void ibl_coordinator_cleanup(IBLCoordinator* coord)
{
	(void)coord;
	g_ibl_coordinator_cleanup_calls++;
}

void light_probe_grid_cleanup(LightProbeGrid* grid)
{
	(void)grid;
	g_light_probe_grid_cleanup_calls++;
}

void shader_destroy(Shader* shader)
{
	if (shader) {
		g_shader_destroy_calls++;
		if (shader->name) {
			free(shader->name);
		}
		if (shader->entries) {
			for (int i = 0; i < shader->entry_count; i++) {
				free(shader->entries[i].name);
			}
			free(shader->entries);
		}
		free(shader);
	}
}

void platform_aligned_free(void* ptr)
{
	if (ptr) {
		g_platform_aligned_free_calls++;
		free(ptr);
	}
}

/* -------------------------------------------------------------------------- */
/*                                 SCENARIOS                                  */
/* -------------------------------------------------------------------------- */

/**
 * Scenario 1: Zero-init
 * Tests that calling scene_cleanup on a zeroed Scene struct does not crash.
 */
static void test_scenario_zero_init(void)
{
	reset_stub_calls();
	mock_gl_reset_calls();

	Scene s = {0};

	/* scene_cleanup should execute cleanly and not access NULL pointers */
	scene_cleanup(&s);

	/* Verify the pointers are still NULL */
	assert(s.gpu == NULL);
	assert(s.shaders == NULL);
	assert(s.simulation == NULL);
	assert(s.visuals == NULL);
}

/**
 * Scenario 2: Partially allocated
 * Tests the robustness of cleanup when only s.gpu is allocated.
 */
static void test_scenario_partially_allocated(void)
{
	reset_stub_calls();
	mock_gl_reset_calls();

	Scene s = {0};
	s.gpu = calloc(1, sizeof(SceneGPUResources));
	assert(s.gpu != NULL);

	/* Set some dummy OpenGL resource handles to verify mock_gl activity */
	s.gpu->icosphere_vao = 10;
	s.gpu->icosphere_vbo = 11;
	s.gpu->hdr_texture = 12;

	scene_cleanup(&s);

	/* Verify cleanup state */
	assert(s.gpu == NULL);
	assert(s.shaders == NULL);
	assert(s.simulation == NULL);
	assert(s.visuals == NULL);

	/* Verify OpenGL mock counts */
	assert(mock_gl_get_delete_buffer_call_count() > 0);
}

/**
 * Scenario 3: Fully allocated
 * Tests that everything is cleaned up properly when fully populated.
 */
static void test_scenario_fully_allocated(void)
{
	reset_stub_calls();
	mock_gl_reset_calls();

	Scene s = {0};
	s.gpu = calloc(1, sizeof(SceneGPUResources));
	s.shaders = calloc(1, sizeof(SceneShaders));
	s.simulation = calloc(1, sizeof(SceneSimulation));
	s.visuals = calloc(1, sizeof(SceneVisuals));

	assert(s.gpu != NULL);
	assert(s.shaders != NULL);
	assert(s.simulation != NULL);
	assert(s.visuals != NULL);

	/* Allocate dummy Shaders to test SHADER_SAFE_DESTROY */
	s.shaders->pbr_instanced = calloc(1, sizeof(Shader));
	s.shaders->pbr_billboard = calloc(1, sizeof(Shader));
	s.shaders->debug = calloc(1, sizeof(Shader));
	s.shaders->debug_line = calloc(1, sizeof(Shader));
	s.shaders->skybox = calloc(1, sizeof(Shader));
#ifdef USE_SSBO_RENDERING
	s.shaders->pbr_ssbo = calloc(1, sizeof(Shader));
#endif

	/* Allocate material library to test material_free_lib */
	s.lighting.material_lib = calloc(1, 100); /* just any pointer */

#ifdef USE_TRANSPARENT_BILLBOARDS
	s.billboard_instances = calloc(5, sizeof(SphereInstance));
	s.billboard_instance_count = 5;
#endif

	/* Allocate dummy HDR files to verify loop cleanup */
	s.hdr_count = 2;
	s.hdr_files = calloc(2, sizeof(char*));
	s.hdr_files[0] = strdup("env1.hdr");
	s.hdr_files[1] = strdup("env2.hdr");

	scene_cleanup(&s);

	/* Verify all sub-structures are freed and set to NULL */
	assert(s.gpu == NULL);
	assert(s.shaders == NULL);
	assert(s.simulation == NULL);
	assert(s.visuals == NULL);
	assert(s.lighting.material_lib == NULL);
	assert(s.hdr_files == NULL);
	assert(s.hdr_count == 0);

	/* Verify dependency call counts */
	assert(g_icosphere_free_calls == 1);
	assert(g_skybox_cleanup_calls == 1);
	assert(g_instanced_group_cleanup_calls == 1);
	assert(g_billboard_renderer_cleanup_calls == 1);
	assert(g_trail_renderer_cleanup_calls == 1);
	assert(g_shockwave_renderer_cleanup_calls == 1);
#ifdef USE_SSBO_RENDERING
	assert(g_ssbo_group_cleanup_calls == 1);
#endif
	assert(g_material_free_lib_calls == 1);
	assert(g_ibl_coordinator_cleanup_calls == 1);
	assert(g_light_probe_grid_cleanup_calls == 1);

	/* 5 standard shaders + pbr_ssbo if defined */
#ifdef USE_SSBO_RENDERING
	assert(g_shader_destroy_calls == 6);
#else
	assert(g_shader_destroy_calls == 5);
#endif

#ifdef USE_TRANSPARENT_BILLBOARDS
	assert(g_platform_aligned_free_calls == 1);
#endif
}

/**
 * Scenario 4: Double call
 * Tests that calling scene_cleanup twice in a row does not crash or
 * double-free.
 */
static void test_scenario_double_call(void)
{
	reset_stub_calls();
	mock_gl_reset_calls();

	Scene s = {0};
	s.gpu = calloc(1, sizeof(SceneGPUResources));
	s.shaders = calloc(1, sizeof(SceneShaders));

	/* First cleanup call */
	scene_cleanup(&s);

	/* Verify first call cleared everything */
	assert(s.gpu == NULL);
	assert(s.shaders == NULL);

	/* Save the call counts */
	int first_shader_destroys = g_shader_destroy_calls;
	int first_material_frees = g_material_free_lib_calls;

	/* Second cleanup call (idempotency check) */
	scene_cleanup(&s);

	/* Verify state is still NULL */
	assert(s.gpu == NULL);
	assert(s.shaders == NULL);

	/* Call counts for sub-structures should not have increased */
	assert(g_shader_destroy_calls == first_shader_destroys);
	assert(g_material_free_lib_calls == first_material_frees);
}

int main(void)
{
	printf("Running test_scenario_zero_init...\n");
	test_scenario_zero_init();
	printf("Running test_scenario_partially_allocated...\n");
	test_scenario_partially_allocated();
	printf("Running test_scenario_fully_allocated...\n");
	test_scenario_fully_allocated();
	printf("Running test_scenario_double_call...\n");
	test_scenario_double_call();
	printf("All tests completed successfully!\n");
	return 0;
}
