#include "icosphere.h"
#include "unity.h"

void setUp(void)
{
	// set stuff up here
}

void tearDown(void)
{
	// clean stuff up here
}

void test_vec3array_init_should_create_empty_array(void)
{
	Vec3Array arr;
	vec3array_init(&arr);
	TEST_ASSERT_NOT_NULL(arr.data);
	TEST_ASSERT_EQUAL_UINT(0, arr.size);
	TEST_ASSERT_GREATER_THAN(0, arr.capacity);
	vec3array_free(&arr);
}

void test_vec3array_push_should_add_elements(void)
{
	Vec3Array arr;
	vec3array_init(&arr);

	vec3 v1 = {1.0f, 2.0f, 3.0f};
	vec3array_push(&arr, v1);

	TEST_ASSERT_EQUAL_UINT(1, arr.size);
	TEST_ASSERT_EQUAL_FLOAT(1.0f, arr.data[0][0]);
	TEST_ASSERT_EQUAL_FLOAT(2.0f, arr.data[0][1]);
	TEST_ASSERT_EQUAL_FLOAT(3.0f, arr.data[0][2]);

	vec3array_free(&arr);
}

void test_icosphere_counts_subdivision_0(void)
{
	IcosphereGeometry geom;
	icosphere_init(&geom);
	icosphere_generate(&geom, 0);

	// Icosahedron: 12 vertices, 20 faces (triangles) -> 60 indices
	TEST_ASSERT_EQUAL_UINT(12, geom.vertices.size);
	TEST_ASSERT_EQUAL_UINT(60, geom.indices.size);

	icosphere_free(&geom);
}

void test_icosphere_counts_subdivision_1(void)
{
	IcosphereGeometry geom;
	icosphere_init(&geom);
	icosphere_generate(&geom, 1);

	// Subdiv 1: Each triangle becomes 4. 20 * 4 = 80 faces -> 240 indices
	// Vertices matches V = 10*F/2 + 2 (Euler characteristic stuff
	// approximate) Actually for icosphere: V = 10 * 4^subdiv + 2 V(1) =
	// 10*4 + 2 = 42

	TEST_ASSERT_EQUAL_UINT(42, geom.vertices.size);
	TEST_ASSERT_EQUAL_UINT(240, geom.indices.size);

	icosphere_free(&geom);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_vec3array_init_should_create_empty_array);
	RUN_TEST(test_vec3array_push_should_add_elements);
	RUN_TEST(test_icosphere_counts_subdivision_0);
	RUN_TEST(test_icosphere_counts_subdivision_1);
	return UNITY_END();
}
