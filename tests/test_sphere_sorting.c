// tests/test_sphere_sorting.c
#include "sphere_sorting.h"
#include "unity.h"
#include <cglm/cglm.h>
#include <stdlib.h>

static const int INIT_CAPACITY = 10;
static const int TEST_COUNT_3 = 3;
static const int TEST_COUNT_4 = 4;
static const int TEST_COUNT_5 = 5;
static const int SMALL_CAPACITY = 2;
static const float COORD_ZERO = 0.0F;
static const float COORD_NEG_5 = -5.0F;
static const float COORD_NEG_10 = -10.0F;
static const float COORD_NEG_20 = -20.0F;
static const float ROUGHNESS_A = 0.1F;
static const float ROUGHNESS_B = 0.2F;
static const float ROUGHNESS_C = 0.3F;
static const float TOLERANCE = 0.01F;

void setUp(void)
{
}
void tearDown(void)
{
}

/* Helper to generate dummy instances */
static SphereInstance* create_dummy_instances(int count)
{
	SphereInstance* instances =
	    (SphereInstance*)calloc((size_t)count, sizeof(SphereInstance));
	for (int i = 0; i < count; ++i) {
		glm_mat4_identity(instances[i].model);
	}
	return instances;
}

static void set_position(SphereInstance* inst, float x_coord, float y_coord,
                         float z_coord)
{
	const int X_INDEX = 3;
	const int Y_INDEX = 3;
	const int Z_INDEX = 3;
	const int X_OFFSET = 0;
	const int Y_OFFSET = 1;
	const int Z_OFFSET = 2;
	inst->model[X_INDEX][X_OFFSET] = x_coord;
	inst->model[Y_INDEX][Y_OFFSET] = y_coord;
	inst->model[Z_INDEX][Z_OFFSET] = z_coord;
}

void test_SphereSorter_Init_ShouldAllocateBuffers(void)
{
	SphereSorter sorter;
	sphere_sorter_init(&sorter, INIT_CAPACITY);

	TEST_ASSERT_NOT_NULL(sorter.entries);
	TEST_ASSERT_NOT_NULL(sorter.temp_instances);
	TEST_ASSERT_EQUAL_INT(INIT_CAPACITY, sorter.capacity);

	sphere_sorter_cleanup(&sorter);
}

void test_SphereSorter_Cleanup_ShouldFreeBuffers(void)
{
	SphereSorter sorter;
	sphere_sorter_init(&sorter, INIT_CAPACITY);
	sphere_sorter_cleanup(&sorter);

	TEST_ASSERT_NULL(sorter.entries);
	TEST_ASSERT_NULL(sorter.temp_instances);
	TEST_ASSERT_EQUAL_INT(0, sorter.capacity);
}

void test_SphereSorter_Sort_ShouldOrderBackToFront(void)
{
	SphereSorter sorter;
	sphere_sorter_init(&sorter, TEST_COUNT_4);

	int count = TEST_COUNT_3;
	SphereInstance* instances = create_dummy_instances(count);

	/* Camera at (0,0,0) looking down -Z */
	vec3 camera_pos = {COORD_ZERO, COORD_ZERO, COORD_ZERO};

	/* Setup spheres at different depths (Z) */
	/* A: Near (-5) */
	set_position(&instances[0], COORD_ZERO, COORD_ZERO, COORD_NEG_5);
	instances[0].roughness = ROUGHNESS_A; /* Marker */

	/* B: Far (-20) */
	set_position(&instances[1], COORD_ZERO, COORD_ZERO, COORD_NEG_20);
	instances[1].roughness = ROUGHNESS_B; /* Marker */

	/* C: Middle (-10) */
	set_position(&instances[2], COORD_ZERO, COORD_ZERO, COORD_NEG_10);
	instances[2].roughness = ROUGHNESS_C; /* Marker */

	/* Expected Order: Far (-20), Middle (-10), Near (-5) */
	/* Indices should become: 1, 2, 0 */

	sphere_sorter_sort(&sorter, instances, count, camera_pos);

	/* Check First (Furthest) */
	const int MAT_Z_INDEX = 3;
	const int MAT_Z_OFFSET = 2;
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, COORD_NEG_20,
	                         instances[0].model[MAT_Z_INDEX][MAT_Z_OFFSET]);
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, ROUGHNESS_B,
	                         instances[0].roughness);

	/* Check Second */
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, COORD_NEG_10,
	                         instances[1].model[MAT_Z_INDEX][MAT_Z_OFFSET]);
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, ROUGHNESS_C,
	                         instances[1].roughness);

	/* Check Third (Nearest) */
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, COORD_NEG_5,
	                         instances[2].model[MAT_Z_INDEX][MAT_Z_OFFSET]);
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, ROUGHNESS_A,
	                         instances[2].roughness);

	free(instances);
	sphere_sorter_cleanup(&sorter);
}

void test_SphereSorter_Resize_ShouldHandleMoreThanCapacity(void)
{
	SphereSorter sorter;
	sphere_sorter_init(&sorter, SMALL_CAPACITY); /* Small capacity */

	int count = TEST_COUNT_5;
	SphereInstance* instances = create_dummy_instances(count);
	vec3 camera_pos = {COORD_ZERO, COORD_ZERO, COORD_ZERO};

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
