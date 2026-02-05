// tests/test_icosphere.c
#include "icosphere.h"
#include "unity.h"

static const float VAL_1 = 1.0F;
static const float VAL_2 = 2.0F;
static const float VAL_3 = 3.0F;

static const unsigned int SUBDIV_0_VERTS = 12;
static const unsigned int SUBDIV_0_INDICES = 60;
static const unsigned int SUBDIV_1_VERTS = 42;
static const unsigned int SUBDIV_1_INDICES = 240;

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

	vec3 vector_one = {VAL_1, VAL_2, VAL_3};
	vec3array_push(&arr, vector_one);

	const unsigned int EXPECTED_SIZE = 1;
	TEST_ASSERT_EQUAL_UINT(EXPECTED_SIZE, arr.size);
	TEST_ASSERT_EQUAL_FLOAT(VAL_1, arr.data[0][0]);
	TEST_ASSERT_EQUAL_FLOAT(VAL_2, arr.data[0][1]);
	TEST_ASSERT_EQUAL_FLOAT(VAL_3, arr.data[0][2]);

	vec3array_free(&arr);
}

void test_icosphere_counts_subdivision_0(void)
{
	IcosphereGeometry geom;
	icosphere_init(&geom);
	const int SUBDIV_LEVEL_0 = 0;
	icosphere_generate(&geom, SUBDIV_LEVEL_0);

	// Icosahedron: 12 vertices, 20 faces (triangles) -> 60 indices
	TEST_ASSERT_EQUAL_UINT(SUBDIV_0_VERTS, geom.vertices.size);
	TEST_ASSERT_EQUAL_UINT(SUBDIV_0_INDICES, geom.indices.size);

	icosphere_free(&geom);
}

void test_icosphere_counts_subdivision_1(void)
{
	IcosphereGeometry geom;
	icosphere_init(&geom);
	const int SUBDIV_LEVEL_1 = 1;
	icosphere_generate(&geom, SUBDIV_LEVEL_1);

	// Subdiv 1: Each triangle becomes 4. 20 * 4 = 80 faces -> 240 indices
	// Vertices matches V = 10*F/2 + 2 (Euler characteristic stuff
	// approximate) Actually for icosphere: V = 10 * 4^subdiv + 2 V(1) =
	// 10*4 + 2 = 42

	TEST_ASSERT_EQUAL_UINT(SUBDIV_1_VERTS, geom.vertices.size);
	TEST_ASSERT_EQUAL_UINT(SUBDIV_1_INDICES, geom.indices.size);

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
