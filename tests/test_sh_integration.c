#include "light_probes.h"
#include "mock_gl_standalone.h"
#include "shader.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Stubs for functions used in light_probes.c but not in this test */
Shader* shader_load(const char* v, const char* f)
{
	return (Shader*)1;
}
void shader_use(Shader* s)
{
}
void shader_destroy(Shader* s)
{
}
void shader_set_mat4(Shader* s, const char* n, const float* v)
{
}
void shader_set_vec3(Shader* s, const char* n, const float* v)
{
}
void shader_set_int(Shader* s, const char* n, int v)
{
}
void shader_set_vec4(Shader* s, const char* n, const float* v)
{
}
GLint shader_get_uniform_location(Shader* s, const char* n)
{
	return 0;
}

/* Mock Test for AABB Calculation */

int main()
{
	printf("Running Light Probe Grid Integration Test (CPU)...\n");

	LightProbeGrid grid;
	memset(&grid, 0, sizeof(LightProbeGrid));

	/* Initialize Mock Spheres */
	int count = 2;
	SphereInstance_POD spheres[2];

	/* Sphere 1 at (0,0,0) with scale 1.0 */
	glm_mat4_identity(spheres[0].model);
	/* Scale 1.0 is default identity */
	/* Radius 1.0. Bounds: -1,-1,-1 to 1,1,1 */

	/* Sphere 2 at (10, 0, 0) with scale 2.0 */
	glm_mat4_identity(spheres[1].model);
	vec3 pos2 = {10.0f, 0.0f, 0.0f};
	glm_translate(spheres[1].model, pos2);
	vec3 scale2 = {2.0f, 2.0f, 2.0f};
	glm_scale(spheres[1].model, scale2);
	/* Radius 2.0. Bounds: 8,-2,-2 to 12,2,2 */

	/* Expected Combined Bounds: */
	/* Min: (-1, -2, -2) */
	/* Max: (12, 2, 2) */

	/* Init Grid */
	light_probe_grid_init_cpu(&grid, 4, 4, 4);

	/* Compute AABB (New Center-Based Logic) */
	light_probe_grid_compute_aabb(&grid, spheres, count,
	                              sizeof(SphereInstance_POD), 1.0f);

	printf("Computed AABB:\n");
	printf("Min: (%.2f, %.2f, %.2f)\n", grid.aabb_min[0], grid.aabb_min[1],
	       grid.aabb_min[2]);
	printf("Max: (%.2f, %.2f, %.2f)\n", grid.aabb_max[0], grid.aabb_max[1],
	       grid.aabb_max[2]);

	/* Assertions (Centers 0 and 10 + Padding 1.0) */
	vec3 expected_min = {-1.0f, -1.0f, -1.0f};
	vec3 expected_max = {11.0f, 1.0f, 1.0f};

	/* Check Min */
	float diff_min[3];
	glm_vec3_sub(grid.aabb_min, expected_min, diff_min);
	if (glm_vec3_norm(diff_min) > 0.001f) {
		printf("FAIL: Min bounds incorrect.\n");
		return 1;
	}

	/* Check Max */
	float diff_max[3];
	glm_vec3_sub(grid.aabb_max, expected_max, diff_max);
	if (glm_vec3_norm(diff_max) > 0.001f) {
		printf("FAIL: Max bounds incorrect.\n");
		return 1;
	}

	/* Check Cell Size (Grid Dim 4x4x4) */
	/* Width X: 12.0 / (4-1) = 4.0 */
	printf("Cell Size: (%.2f, %.2f, %.2f)\n", grid.cell_size[0],
	       grid.cell_size[1], grid.cell_size[2]);

	if (fabs(grid.cell_size[0] - 4.0f) > 0.001f) {
		printf("FAIL: Cell size X incorrect.\n");
		return 1;
	}

	light_probe_grid_free_cpu(&grid);

	printf("Integration Test Passed.\n");
	return 0;
}
