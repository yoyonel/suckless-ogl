#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "billboard_rendering.h"
#include "mock_gl_standalone.h"
#include "unity.h"

/* Mock render_utils function */
void render_utils_setup_sphere_instance_attributes(GLsizei stride,
                                                   size_t offset_albedo,
                                                   size_t offset_metallic)
{
	(void)stride;
	(void)offset_albedo;
	(void)offset_metallic;
}

#ifndef SYNC_ATTR_START
#define SYNC_ATTR_START 10
#endif
#ifndef MAX_VERTEX_ATTRIBS_BASELINE
#define MAX_VERTEX_ATTRIBS_BASELINE 16
#endif

/* Include source directly to access internal state if needed */
#include "billboard_rendering.c"

void setUp(void)
{
	mock_gl_reset_calls();
}

void tearDown(void)
{
}

void test_billboard_group_update_handles_overflow(void)
{
	BillboardGroup group = {0};
	SphereInstance instances[2]; /* Initial capacity 2 */
	SphereInstance large_instances[4]; /* New count 4 */

	/* Initialize the group with 2 instances */
	billboard_group_init(&group, instances, 2);

	/* Reset mocks to clear the glBufferData call from init */
	mock_gl_reset_calls();

	/* Update with 4 instances - should trigger reallocation */
	billboard_group_update(&group, large_instances, 4);

	/* Verify reallocation happened */
	/* glBufferData should be called once (reallocation) */
	TEST_ASSERT_EQUAL_MESSAGE(1, mock_gl_get_buffer_data_call_count(),
	                          "Expected glBufferData (reallocation) to be called");

	/* glBufferSubData should NOT be called */
	TEST_ASSERT_EQUAL_MESSAGE(0, mock_gl_get_buffer_sub_data_call_count(),
	                          "Expected glBufferSubData NOT to be called");

	/* Verify size */
	TEST_ASSERT_EQUAL_MESSAGE(4 * sizeof(SphereInstance),
	                          mock_gl_get_last_buffer_data_size(),
	                          "Expected buffer size to match new count");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_billboard_group_update_handles_overflow);
	return UNITY_END();
}
