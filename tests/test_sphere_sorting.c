#include "sphere_sorting.h"
#include "unity.h"
#include <cglm/cglm.h>
#include <stdlib.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* Helper to generate dummy instances */
static SphereInstance* create_dummy_instances(int count)
{
	SphereInstance* instances = calloc(count, sizeof(SphereInstance));
	for (int i = 0; i < count; ++i) {
		glm_mat4_identity(instances[i].model);
	}
	return instances;
}

static void set_position(SphereInstance* inst, float x, float y, float z)
{
	inst->model[3][0] = x;
	inst->model[3][1] = y;
	inst->model[3][2] = z;
}

void test_SphereSorter_Init_ShouldAllocateBuffers(void)
{
	SphereSorter sorter;
	sphere_sorter_init(&sorter, 10);

	TEST_ASSERT_NOT_NULL(sorter.entries);
	TEST_ASSERT_NOT_NULL(sorter.temp_instances);
	TEST_ASSERT_EQUAL_INT(10, sorter.capacity);

	sphere_sorter_cleanup(&sorter);
}

void test_SphereSorter_Cleanup_ShouldFreeBuffers(void)
{
	SphereSorter sorter;
	sphere_sorter_init(&sorter, 10);
	sphere_sorter_cleanup(&sorter);

	TEST_ASSERT_NULL(sorter.entries);
	TEST_ASSERT_NULL(sorter.temp_instances);
	TEST_ASSERT_EQUAL_INT(0, sorter.capacity);
}

void test_SphereSorter_Sort_ShouldOrderBackToFront(void)
{
	SphereSorter sorter;
	sphere_sorter_init(&sorter, 4);

	int count = 3;
	SphereInstance* instances = create_dummy_instances(count);

	/* Camera at (0,0,0) looking down -Z */
	vec3 camera_pos = {0.0f, 0.0f, 0.0f};

	/* Setup spheres at different depths (Z) */
	/* A: Near (-5) */
	set_position(&instances[0], 0.0f, 0.0f, -5.0f);
	instances[0].roughness = 0.1f; /* Marker */

	/* B: Far (-20) */
	set_position(&instances[1], 0.0f, 0.0f, -20.0f);
	instances[1].roughness = 0.2f; /* Marker */

	/* C: Middle (-10) */
	set_position(&instances[2], 0.0f, 0.0f, -10.0f);
	instances[2].roughness = 0.3f; /* Marker */

	/* Expected Order: Far (-20), Middle (-10), Near (-5) */
	/* Indices should become: 1, 2, 0 */

	sphere_sorter_sort(&sorter, instances, count, camera_pos);

	/* Check First (Furthest) */
	TEST_ASSERT_FLOAT_WITHIN(0.01f, -20.0f, instances[0].model[3][2]);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, instances[0].roughness);

	/* Check Second */
	TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, instances[1].model[3][2]);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, instances[1].roughness);

	/* Check Third (Nearest) */
	TEST_ASSERT_FLOAT_WITHIN(0.01f, -5.0f, instances[2].model[3][2]);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.1f, instances[2].roughness);

	free(instances);
	sphere_sorter_cleanup(&sorter);
}

void test_SphereSorter_Resize_ShouldHandleMoreThanCapacity(void)
{
	SphereSorter sorter;
	sphere_sorter_init(&sorter, 2); /* Small capacity */

	int count = 5;
	SphereInstance* instances = create_dummy_instances(count);
	vec3 camera_pos = {0.0f, 0.0f, 0.0f};

	/* Just check it doesn't crash and resizes */
	sphere_sorter_sort(&sorter, instances, count, camera_pos);

	TEST_ASSERT_GREATER_OR_EQUAL(count, sorter.capacity);

	free(instances);
	sphere_sorter_cleanup(&sorter);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_SphereSorter_Init_ShouldAllocateBuffers);
	RUN_TEST(test_SphereSorter_Cleanup_ShouldFreeBuffers);
	RUN_TEST(test_SphereSorter_Sort_ShouldOrderBackToFront);
	RUN_TEST(test_SphereSorter_Resize_ShouldHandleMoreThanCapacity);
	return UNITY_END();
}
