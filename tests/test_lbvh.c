#include "lbvh.h"
#include "unity.h"
#include <cglm/cglm.h>
#include <string.h>

enum { INITIAL_CAPACITY = 10 };
enum { BVH_CAP_EXPECTED = 20 };
enum { SCRATCH_CAP_EXPECTED = 10 };
enum { SMALL_CAPACITY = 2 };
enum { RESIZE_COUNT = 5 };
enum { RESIZE_CAP_EXPECTED = 10 };

void setUp(void)
{
}
void tearDown(void)
{
}

void test_LBVH_Init_ShouldAllocateBuffers(void)
{
	LBVH bvh;
	TEST_ASSERT_EQUAL_INT(1, lbvh_init(&bvh, INITIAL_CAPACITY));
	TEST_ASSERT_NOT_NULL(bvh.nodes);
	TEST_ASSERT_NOT_NULL(bvh.morton_codes);
	TEST_ASSERT_NOT_NULL(bvh.proxies);
	TEST_ASSERT_NOT_NULL(bvh.sorted_spheres);
	TEST_ASSERT_EQUAL_INT(BVH_CAP_EXPECTED, bvh.capacity);
	TEST_ASSERT_EQUAL_INT(SCRATCH_CAP_EXPECTED, bvh.scratch_capacity);
	lbvh_cleanup(&bvh);
}

void test_Morton_Calculation(void)
{
	vec3 min = {-1.0F, -1.0F, -1.0F};
	vec3 max = {1.0F, 1.0F, 1.0F};
	vec3 pos = {0.0F, 0.0F, 0.0F};

	uint32_t code = calculate_morton_3d(pos, min, max);
	TEST_ASSERT_NOT_EQUAL(0, code);

	/* Same point should have same code */
	TEST_ASSERT_EQUAL_UINT32(code, calculate_morton_3d(pos, min, max));

	/* Different points should have different codes */
	float half = 0.5F;
	vec3 pos2 = {half, half, half};
	TEST_ASSERT_NOT_EQUAL(code, calculate_morton_3d(pos2, min, max));
}

void test_LBVH_Build_ShouldNotCrash(void)
{
	LBVH bvh;
	lbvh_init(&bvh, 4);

	SphereInstance instances[3];
	memset(instances, 0, sizeof(instances));
	for (int i = 0; i < 3; i++) {
		glm_mat4_identity(instances[i].model);
		instances[i].model[3][0] = (float)i;
	}

	lbvh_build(&bvh, instances, 3);
	TEST_ASSERT_GREATER_THAN(0, bvh.node_count);

	lbvh_cleanup(&bvh);
}

void test_LBVH_Resize_ShouldHandleMoreThanCapacity(void)
{
	LBVH bvh;
	lbvh_init(&bvh, SMALL_CAPACITY); /* Small capacity */

	SphereInstance instances[RESIZE_COUNT];
	memset(instances, 0, sizeof(instances));
	for (int i = 0; i < RESIZE_COUNT; i++) {
		glm_mat4_identity(instances[i].model);
		instances[i].model[3][1] = (float)i;
	}

	lbvh_build(&bvh, instances, RESIZE_COUNT);
	TEST_ASSERT_GREATER_OR_EQUAL(RESIZE_COUNT, bvh.scratch_capacity);
	TEST_ASSERT_GREATER_OR_EQUAL(RESIZE_CAP_EXPECTED, bvh.capacity);

	lbvh_cleanup(&bvh);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_LBVH_Init_ShouldAllocateBuffers);
	RUN_TEST(test_Morton_Calculation);
	RUN_TEST(test_LBVH_Build_ShouldNotCrash);
	RUN_TEST(test_LBVH_Resize_ShouldHandleMoreThanCapacity);
	return UNITY_END();
}
