/**
 * @file test_scene_init_rollback.c
 * @brief Unit test verifying that scene_init rollback (goto cleanup) calls
 * scene_cleanup exactly once upon failure.
 */

#include "billboard_renderer.h"
#include "ibl_coordinator.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "light_probes.h"
#include "material.h"
#include "mock_gl_standalone.h"
#include "platform/platform_fs.h"
#include "scene.h"
#include "scene_gpu_resources.h"
#include "scene_shaders.h"
#include "scene_simulation.h"
#include "scene_visuals.h"
#include "shader.h"
#include "skybox.h"
#include "ssbo_rendering.h"
#include "unity.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Define OpenGL macros missing in the test mock Glad header */
#ifndef GL_UNIFORM_BUFFER
#define GL_UNIFORM_BUFFER 0x8A11
#endif
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT 0x0080
#endif

/* --- Counters for mocks --- */
static int mock_scene_cleanup_calls = 0;
static int mock_shader_load_calls = 0;

/* --- Mock scene_cleanup --- */
void scene_cleanup(Scene* scene)
{
	(void)scene;
	mock_scene_cleanup_calls++;
}

/* --- Mocks for dependencies --- */
bool asset_has_flag(const char* filename, uint32_t flag)
{
	(void)filename;
	(void)flag;
	return true;
}

Shader* shader_load(const char* vertex_path, const char* fragment_path)
{
	(void)vertex_path;
	(void)fragment_path;
	mock_shader_load_calls++;
	return NULL; /* Simulate failure */
}

GLuint shader_load_compute(const char* compute_path)
{
	(void)compute_path;
	return 0;
}

void shader_use(Shader* shader)
{
	(void)shader;
}

GLint shader_get_uniform_location(Shader* shader, const char* name)
{
	(void)shader;
	(void)name;
	return -1;
}

bool platform_dir_list(const char* path, PlatformDirCallback callback,
                       void* user_data)
{
	(void)path;
	(void)callback;
	(void)user_data;
	return true;
}

void instanced_group_init(InstancedGroup* group, const SphereInstance* data,
                          int count)
{
	(void)group;
	(void)data;
	(void)count;
}

void instanced_group_bind_mesh(InstancedGroup* group, GLuint vbo, GLuint nbo,
                               GLuint ebo)
{
	(void)group;
	(void)vbo;
	(void)nbo;
	(void)ebo;
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

void light_probe_grid_set_scene(LightProbeGrid* grid, const void* data,
                                int count, size_t stride)
{
	(void)grid;
	(void)data;
	(void)count;
	(void)stride;
}

void light_probe_grid_compute_aabb(LightProbeGrid* grid, const void* data,
                                   int count, size_t stride, float padding)
{
	(void)grid;
	(void)data;
	(void)count;
	(void)stride;
	(void)padding;
}

void light_probe_grid_update_async(LightProbeGrid* grid)
{
	(void)grid;
}

void light_probe_grid_init(LightProbeGrid* grid, int dim_x, int dim_y,
                           int dim_z)
{
	(void)grid;
	(void)dim_x;
	(void)dim_y;
	(void)dim_z;
}

#ifdef USE_SSBO_RENDERING
void ssbo_group_init(SSBOGroup* group, const SphereInstanceSSBO* data,
                     int count)
{
	(void)group;
	(void)data;
	(void)count;
}

void ssbo_group_bind_mesh(SSBOGroup* group, GLuint vbo, GLuint nbo, GLuint ebo)
{
	(void)group;
	(void)vbo;
	(void)nbo;
	(void)ebo;
}
#endif

GLuint render_utils_create_color_texture(float r, float g, float b, float a)
{
	(void)r;
	(void)g;
	(void)b;
	(void)a;
	return 1;
}

GLuint build_brdf_lut_map(int size)
{
	(void)size;
	return 2;
}

void ibl_coordinator_init(IBLCoordinator* coord, GLuint spmap, GLuint irmap,
                          GLuint lum_pass1, GLuint lum_pass2)
{
	(void)coord;
	(void)spmap;
	(void)irmap;
	(void)lum_pass1;
	(void)lum_pass2;
}

void render_utils_create_empty_vao(GLuint* vao)
{
	if (vao)
		*vao = 3;
}

void render_utils_create_quad_vbo(GLuint* vbo)
{
	if (vbo)
		*vbo = 4;
}

void render_utils_create_wire_cube_vbo(GLuint* vbo)
{
	if (vbo)
		*vbo = 5;
}

void render_utils_create_wire_quad_vbo(GLuint* vbo)
{
	if (vbo)
		*vbo = 6;
}

void skybox_init(Skybox* skybox, Shader* shader)
{
	(void)skybox;
	(void)shader;
}

void icosphere_init(IcosphereGeometry* geometry)
{
	(void)geometry;
}

MaterialLib* material_load_presets(const char* filepath)
{
	(void)filepath;
	return NULL;
}

/* Include the source file under test */
#include "scene_init.c"

/* --- Setup/Teardown --- */
void setUp(void)
{
	mock_scene_cleanup_calls = 0;
	mock_shader_load_calls = 0;
}

void tearDown(void)
{
}

/* --- Test Case --- */
void test_scene_init_rollback_on_shader_failure(void)
{
	Scene scene;
	memset(&scene, 0, sizeof(Scene));

	/* Call scene_init, which should fail due to shader_load returning NULL
	 */
	int ret = scene_init(&scene);

	/* 1. Verify return code is 0 (failure) */
	TEST_ASSERT_EQUAL_INT(0, ret);

	/* 2. Verify scene_cleanup was called exactly once */
	TEST_ASSERT_EQUAL_INT(1, mock_scene_cleanup_calls);

	/* 3. Verify shader_load was called to trigger the failure */
	TEST_ASSERT_TRUE(mock_shader_load_calls > 0);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_scene_init_rollback_on_shader_failure);
	return UNITY_END();
}
