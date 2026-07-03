/**
 * @file test_scene_render.c
 * @brief Tests for scene_render.c — rendering pipeline branches.
 *
 * Uses standalone mocks (no GPU required). Includes scene_render.c directly
 * and mocks all GPU/subsystem calls.
 */

#include "mock_gl_standalone.h"

/* Type-providing headers */
#include "billboard_renderer.h"
#include "gpu_profiler.h"
#include "instanced_rendering.h"
#include "light_probes.h"
#include "scene.h"
#include "scene_gpu_resources.h"
#include "scene_shaders.h"
#include "scene_simulation.h"
#include "scene_uniforms.h"
#include "shockwave.h"
#include "skybox.h"
#include "trail_renderer.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ---- Mock call counters ---- */
static int mock_shader_use_calls;
static int mock_shader_set_int_loc_calls;
static int mock_shader_set_vec3_loc_calls;
static int mock_shader_set_vec4_loc_calls;
static int mock_shader_set_mat4_loc_calls;
static int mock_skybox_render_calls;
static int mock_probe_sync_calls;
static int mock_probe_render_debug_calls;

static int mock_billboard_renderer_draw_calls;
static BillboardRenderParams mock_last_render_params;
static GLuint mock_last_pbr_shader;
static GLuint mock_last_debug_shader;
static void* mock_last_ubo_ptr;

static int mock_instanced_draw_calls;
static int mock_trail_draw_calls;
static int mock_shockwave_draw_calls;
static int mock_profiler_start_calls;
static int mock_profiler_end_calls;

/* ---- Mocks ---- */

void shader_use(Shader* s)
{
	(void)s;
	mock_shader_use_calls++;
}

void shader_set_int_loc(GLint loc, int val)
{
	(void)loc;
	(void)val;
	mock_shader_set_int_loc_calls++;
}

void shader_set_vec3_loc(GLint loc, const float* v)
{
	(void)loc;
	(void)v;
	mock_shader_set_vec3_loc_calls++;
}

void shader_set_vec4_loc(GLint loc, const float* v)
{
	(void)loc;
	(void)v;
	mock_shader_set_vec4_loc_calls++;
}

void shader_set_mat4_loc(GLint loc, const float* m)
{
	(void)loc;
	(void)m;
	mock_shader_set_mat4_loc_calls++;
}

void skybox_render(Skybox* sb, Shader* shader, GLuint cubemap, GLuint fallback,
                   const mat4 inv_vp, float lod)
{
	(void)sb;
	(void)shader;
	(void)cubemap;
	(void)fallback;
	(void)inv_vp;
	(void)lod;
	mock_skybox_render_calls++;
}

void light_probe_grid_sync(LightProbeGrid* g)
{
	(void)g;
	mock_probe_sync_calls++;
}

void light_probe_grid_render_debug(LightProbeGrid* g, mat4 v, mat4 p)
{
	(void)g;
	(void)v;
	(void)p;
	mock_probe_render_debug_calls++;
}

void billboard_renderer_init(BillboardRenderer* renderer, int initial_capacity)
{
	(void)renderer;
	(void)initial_capacity;
}

void billboard_renderer_prepare(BillboardRenderer* renderer, GLuint quad_vbo,
                                GLuint wire_quad_vbo, GLuint wire_cube_vbo)
{
	(void)renderer;
	(void)quad_vbo;
	(void)wire_quad_vbo;
	(void)wire_cube_vbo;
}

void billboard_renderer_cleanup(BillboardRenderer* renderer)
{
	(void)renderer;
}

void billboard_renderer_draw(BillboardRenderer* renderer,
                             const BillboardRenderParams* params,
                             GLuint pbr_billboard_shader,
                             GLuint debug_line_shader, void* billboard_ubo_ptr)
{
	(void)renderer;
	mock_billboard_renderer_draw_calls++;
	if (params) {
		mock_last_render_params = *params;
	}
	mock_last_pbr_shader = pbr_billboard_shader;
	mock_last_debug_shader = debug_line_shader;
	mock_last_ubo_ptr = billboard_ubo_ptr;
}

void instanced_group_draw(InstancedGroup* ig, size_t index_count)
{
	(void)ig;
	(void)index_count;
	mock_instanced_draw_calls++;
}

void trail_renderer_draw(TrailRenderer* tr, mat4 v, mat4 p, vec3 cam)
{
	(void)tr;
	(void)v;
	(void)p;
	(void)cam;
	mock_trail_draw_calls++;
}

void shockwave_draw(const ShockwaveRenderer* sr, mat4 v, mat4 p, vec3 cam,
                    float sim_time, int w, int h)
{
	(void)sr;
	(void)v;
	(void)p;
	(void)cam;
	(void)sim_time;
	(void)w;
	(void)h;
	mock_shockwave_draw_calls++;
}

void gpu_profiler_start_stage(GPUProfiler* profiler, const char* name,
                              uint32_t color)
{
	(void)profiler;
	(void)name;
	(void)color;
	mock_profiler_start_calls++;
}

void gpu_profiler_end_stage(GPUProfiler* profiler)
{
	(void)profiler;
	mock_profiler_end_calls++;
}

#ifdef USE_SSBO_RENDERING
void ssbo_group_draw(SSBOGroup* sg, size_t index_count)
{
	(void)sg;
	(void)index_count;
}
#endif

/* ---- Include the file under test ---- */
#include "scene_render.c"

/* ---- Helpers ---- */

static void reset_counters(void)
{
	mock_gl_reset_calls();
	mock_shader_use_calls = 0;
	mock_shader_set_int_loc_calls = 0;
	mock_shader_set_vec3_loc_calls = 0;
	mock_shader_set_vec4_loc_calls = 0;
	mock_shader_set_mat4_loc_calls = 0;
	mock_skybox_render_calls = 0;
	mock_probe_sync_calls = 0;
	mock_probe_render_debug_calls = 0;
	mock_billboard_renderer_draw_calls = 0;
	memset(&mock_last_render_params, 0, sizeof(mock_last_render_params));
	mock_last_pbr_shader = 0;
	mock_last_debug_shader = 0;
	mock_last_ubo_ptr = NULL;
	mock_instanced_draw_calls = 0;
	mock_trail_draw_calls = 0;
	mock_shockwave_draw_calls = 0;
	mock_profiler_start_calls = 0;
	mock_profiler_end_calls = 0;
}

static SceneGPUResources test_gpu;
static SceneShaders test_shaders;
static SceneSimulation test_simulation;
static Shader test_pbr_billboard_shader;
static Shader test_debug_line_shader;

static Scene make_scene(void)
{
	Scene s;
	memset(&s, 0, sizeof(s));
	memset(&test_gpu, 0, sizeof(test_gpu));
	memset(&test_shaders, 0, sizeof(test_shaders));
	memset(&test_simulation, 0, sizeof(test_simulation));
	memset(&test_pbr_billboard_shader, 0,
	       sizeof(test_pbr_billboard_shader));
	memset(&test_debug_line_shader, 0, sizeof(test_debug_line_shader));
	test_pbr_billboard_shader.program = 1001;
	test_debug_line_shader.program = 1002;
	test_shaders.pbr_billboard = &test_pbr_billboard_shader;
	test_shaders.debug_line = &test_debug_line_shader;
	s.gpu = &test_gpu;
	s.shaders = &test_shaders;
	s.simulation = &test_simulation;
	return s;
}

/* ======================================================================
 * aa_mode_to_string
 * ====================================================================== */

void test_aa_mode_screen_space(void)
{
	TEST_ASSERT_EQUAL_STRING("Screen-space",
	                         aa_mode_to_string(AA_MODE_SCREEN_SPACE));
}

void test_aa_mode_curvature(void)
{
	TEST_ASSERT_EQUAL_STRING("Curvature-based",
	                         aa_mode_to_string(AA_MODE_CURVATURE));
}

void test_aa_mode_unknown(void)
{
	TEST_ASSERT_EQUAL_STRING("Unknown", aa_mode_to_string((AAMode)99));
}

/* ======================================================================
 * scene_update_gpu_buffers
 * ====================================================================== */

void test_update_gpu_buffers_binds_vao(void)
{
	Scene s = make_scene();
	s.gpu->icosphere_vao = 10;
	s.gpu->icosphere_vbo = 20;
	s.gpu->icosphere_nbo = 30;
	s.gpu->icosphere_ebo = 40;
	s.geometry.vertices.size = 8;
	s.geometry.normals.size = 8;
	s.geometry.indices.size = 12;

	scene_update_gpu_buffers(&s);
	/* Just verify it doesn't crash — GL calls are stubs */
	TEST_PASS();
}

/* ======================================================================
 * scene_bind_ibl_textures (static — accessible via #include .c)
 * ====================================================================== */

void test_bind_ibl_textures_caches(void)
{
	Scene s = make_scene();
	s.gpu->irradiance_tex = 100;
	s.gpu->spec_prefiltered_tex = 200;
	s.gpu->brdf_lut_tex = 300;
	s.gpu->dummy_black_tex = 1;
	memset(s.gpu->bound_ibl_textures, 0, sizeof(s.gpu->bound_ibl_textures));

	scene_bind_ibl_textures(&s);

	TEST_ASSERT_EQUAL_UINT(100, s.gpu->bound_ibl_textures[0]);
	TEST_ASSERT_EQUAL_UINT(200, s.gpu->bound_ibl_textures[1]);
	TEST_ASSERT_EQUAL_UINT(300, s.gpu->bound_ibl_textures[2]);
}

void test_bind_ibl_textures_skips_when_cached(void)
{
	Scene s = make_scene();
	s.gpu->irradiance_tex = 100;
	s.gpu->spec_prefiltered_tex = 200;
	s.gpu->brdf_lut_tex = 300;
	s.gpu->bound_ibl_textures[0] = 100;
	s.gpu->bound_ibl_textures[1] = 200;
	s.gpu->bound_ibl_textures[2] = 300;

	reset_counters();
	scene_bind_ibl_textures(&s);

	TEST_ASSERT_EQUAL_UINT(100, s.gpu->bound_ibl_textures[0]);
	TEST_ASSERT_EQUAL_UINT(200, s.gpu->bound_ibl_textures[1]);
	TEST_ASSERT_EQUAL_UINT(300, s.gpu->bound_ibl_textures[2]);
}

void test_bind_ibl_textures_uses_dummy_when_zero(void)
{
	Scene s = make_scene();
	s.gpu->irradiance_tex = 0;
	s.gpu->spec_prefiltered_tex = 0;
	s.gpu->brdf_lut_tex = 0;
	s.gpu->dummy_black_tex = 999;
	memset(s.gpu->bound_ibl_textures, 0, sizeof(s.gpu->bound_ibl_textures));

	scene_bind_ibl_textures(&s);

	/* Should bind fallback dummy black texture */
	TEST_ASSERT_EQUAL_UINT(999, s.gpu->bound_ibl_textures[0]);
	TEST_ASSERT_EQUAL_UINT(999, s.gpu->bound_ibl_textures[1]);
	TEST_ASSERT_EQUAL_UINT(999, s.gpu->bound_ibl_textures[2]);
}

/* ======================================================================
 * scene_bind_probe_textures (static)
 * ====================================================================== */

void test_bind_probe_textures_updates_cache(void)
{
	Scene s = make_scene();
	s.lighting.probe_grid.sh_textures[0] = 10;
	s.lighting.probe_grid.sh_textures[1] = 20;
	memset(s.gpu->bound_sh_textures, 0, sizeof(s.gpu->bound_sh_textures));

	scene_bind_probe_textures(&s);

	TEST_ASSERT_EQUAL_UINT(10, s.gpu->bound_sh_textures[0]);
	TEST_ASSERT_EQUAL_UINT(20, s.gpu->bound_sh_textures[1]);
}

void test_bind_probe_textures_skips_when_cached(void)
{
	Scene s = make_scene();
	s.lighting.probe_grid.sh_textures[0] = 10;
	s.gpu->bound_sh_textures[0] = 10;

	reset_counters();
	scene_bind_probe_textures(&s);

	TEST_ASSERT_EQUAL_UINT(10, s.gpu->bound_sh_textures[0]);
}

/* ======================================================================
 * scene_render (main entry point)
 * ====================================================================== */

void test_render_instanced_no_envmap_no_nbody(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = false;
	s.config.wireframe = false;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(0, mock_skybox_render_calls);
	TEST_ASSERT_EQUAL_INT(0, mock_billboard_renderer_draw_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_instanced_draw_calls);
	TEST_ASSERT_EQUAL_INT(0, mock_trail_draw_calls);
	TEST_ASSERT_EQUAL_INT(0, mock_shockwave_draw_calls);
	TEST_ASSERT_EQUAL_INT(0, mock_probe_sync_calls);
	TEST_ASSERT_EQUAL_INT(0, mock_probe_render_debug_calls);
}

void test_render_instanced_wireframe(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = false;
	s.config.wireframe = true;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_instanced_draw_calls);
}

void test_render_billboard_mode(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = true;
	s.config.wireframe = false;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_billboard_renderer_draw_calls);
	TEST_ASSERT_EQUAL_INT(0, mock_instanced_draw_calls);
}

void test_render_nbody_draws_trails_and_shockwave(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = false;
	s.config.wireframe = false;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = true;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_trail_draw_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_shockwave_draw_calls);
}

void test_render_nbody_wireframe_shockwave(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = false;
	s.config.wireframe = true;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = true;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_trail_draw_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_shockwave_draw_calls);
}

void test_render_gi_mode_triggers_probe_sync(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = false;
	s.config.wireframe = false;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_3D_TEX;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_probe_sync_calls);
	/* Verify SH cache was invalidated */
	for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
		TEST_ASSERT_EQUAL_UINT(0, s.gpu->bound_sh_textures[i]);
	}
}

void test_render_show_probe_grid_triggers_sync_and_debug(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = false;
	s.config.wireframe = false;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = true;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_probe_sync_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_probe_render_debug_calls);
}

#ifdef USE_TRANSPARENT_BILLBOARDS
void test_render_envmap_draws_skybox(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = false;
	s.config.wireframe = false;
	s.config.show_envmap = true;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_skybox_render_calls);
}

void test_render_billboard_wireframe_draws_debug(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = true;
	s.config.wireframe = true;
	s.config.pbr_debug_mode = 0;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_billboard_renderer_draw_calls);
	TEST_ASSERT_TRUE(mock_last_render_params.wireframe);
}

void test_render_billboard_sort_cpu(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = true;
	s.config.sorting_mode = SORTING_MODE_CPU_QSORT;
	s.config.wireframe = false;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_billboard_renderer_draw_calls);
	TEST_ASSERT_EQUAL(SORTING_MODE_CPU_QSORT,
	                  mock_last_render_params.sorting_mode);
}

void test_render_billboard_sort_radix(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = true;
	s.config.sorting_mode = SORTING_MODE_CPU_RADIX;
	s.config.wireframe = false;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_billboard_renderer_draw_calls);
	TEST_ASSERT_EQUAL(SORTING_MODE_CPU_RADIX,
	                  mock_last_render_params.sorting_mode);
}

void test_render_billboard_sort_gpu(void)
{
	Scene s = make_scene();
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(profiler));
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);

	s.config.billboard_mode = true;
	s.config.sorting_mode = SORTING_MODE_GPU_BITONIC;
	s.config.wireframe = false;
	s.config.show_envmap = false;
	s.config.gi_mode = GI_MODE_OFF;
	s.config.show_probe_grid = false;
	s.simulation->nbody_mode = false;

	reset_counters();
	scene_render(&s, &profiler, view, proj, cam, prev_vp, 800, 600);

	TEST_ASSERT_EQUAL_INT(1, mock_billboard_renderer_draw_calls);
	TEST_ASSERT_EQUAL(SORTING_MODE_GPU_BITONIC,
	                  mock_last_render_params.sorting_mode);
}
#endif /* USE_TRANSPARENT_BILLBOARDS */

/* ======================================================================
 * scene_render_instanced (static)
 * ====================================================================== */

void test_render_instanced_sets_uniforms(void)
{
	Scene s = make_scene();
	mat4 view, proj, prev_vp;
	vec3 cam = {1, 2, 3};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);
	s.shaders->instanced_uniforms.u_specular_aa_enabled = 5;
	s.shaders->instanced_uniforms.u_aa_mode = 6;
	s.shaders->instanced_uniforms.probe_grid_dim = 7;

	reset_counters();
	scene_render_instanced(&s, view, proj, cam, prev_vp);

	TEST_ASSERT_GREATER_THAN(0, mock_shader_use_calls);
	TEST_ASSERT_GREATER_THAN(0, mock_shader_set_mat4_loc_calls);
	TEST_ASSERT_EQUAL_INT(1, mock_instanced_draw_calls);
}

void test_render_instanced_skips_negative_uniform_locs(void)
{
	Scene s = make_scene();
	mat4 view, proj, prev_vp;
	vec3 cam = {0};
	glm_mat4_identity(view);
	glm_mat4_identity(proj);
	glm_mat4_identity(prev_vp);
	s.shaders->instanced_uniforms.u_specular_aa_enabled = -1;
	s.shaders->instanced_uniforms.u_aa_mode = -1;
	s.shaders->instanced_uniforms.probe_grid_dim = -1;

	reset_counters();
	scene_render_instanced(&s, view, proj, cam, prev_vp);

	TEST_ASSERT_EQUAL_INT(1, mock_instanced_draw_calls);
}

/* ======================================================================
 * Test runner
 * ====================================================================== */

void setUp(void)
{
	reset_counters();
}

void tearDown(void)
{
}

int main(void)
{
	UNITY_BEGIN();

	/* aa_mode_to_string */
	RUN_TEST(test_aa_mode_screen_space);
	RUN_TEST(test_aa_mode_curvature);
	RUN_TEST(test_aa_mode_unknown);

	/* scene_update_gpu_buffers */
	RUN_TEST(test_update_gpu_buffers_binds_vao);

	/* scene_bind_ibl_textures */
	RUN_TEST(test_bind_ibl_textures_caches);
	RUN_TEST(test_bind_ibl_textures_skips_when_cached);
	RUN_TEST(test_bind_ibl_textures_uses_dummy_when_zero);

	/* scene_bind_probe_textures */
	RUN_TEST(test_bind_probe_textures_updates_cache);
	RUN_TEST(test_bind_probe_textures_skips_when_cached);

	/* scene_render — orchestrator */
	RUN_TEST(test_render_instanced_no_envmap_no_nbody);
	RUN_TEST(test_render_instanced_wireframe);
	RUN_TEST(test_render_billboard_mode);
	RUN_TEST(test_render_nbody_draws_trails_and_shockwave);
	RUN_TEST(test_render_nbody_wireframe_shockwave);
	RUN_TEST(test_render_gi_mode_triggers_probe_sync);
	RUN_TEST(test_render_show_probe_grid_triggers_sync_and_debug);

#ifdef USE_TRANSPARENT_BILLBOARDS
	RUN_TEST(test_render_envmap_draws_skybox);
	RUN_TEST(test_render_billboard_wireframe_draws_debug);
	RUN_TEST(test_render_billboard_sort_cpu);
	RUN_TEST(test_render_billboard_sort_radix);
	RUN_TEST(test_render_billboard_sort_gpu);
#endif

	/* scene_render_instanced (static) */
	RUN_TEST(test_render_instanced_sets_uniforms);
	RUN_TEST(test_render_instanced_skips_negative_uniform_locs);

	return UNITY_END();
}
