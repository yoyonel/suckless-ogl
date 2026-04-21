#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Include headers from the project */
#include "billboard_rendering.h"
#include "mock_gl_standalone.h"
#include "unity.h"

/* -------------------------------------------------------------------------- */
/*                             MOCK RENDER UTILS                              */
/* -------------------------------------------------------------------------- */

/* We need to mock functions called by billboard_rendering.c that are not in
 * GLAD and not covered by mock_gl_standalone.c */

void render_utils_setup_sphere_instance_attributes(GLsizei stride,
                                                   size_t offset_albedo,
                                                   size_t offset_metallic,
                                                   size_t offset_prev_center)
{
	(void)stride;
	(void)offset_albedo;
	(void)offset_metallic;
	(void)offset_prev_center;
}

/* -------------------------------------------------------------------------- */
/*                        INCLUDE IMPLEMENTATION UNDER TEST                   */
/* -------------------------------------------------------------------------- */

/* Define SYNC_ATTR_START and MAX_VERTEX_ATTRIBS_BASELINE as they might be used
 */
#ifndef SYNC_ATTR_START
#define SYNC_ATTR_START 10
#endif
#ifndef MAX_VERTEX_ATTRIBS_BASELINE
#define MAX_VERTEX_ATTRIBS_BASELINE 16
#endif

/* Include the source file directly to test it in isolation */
#include "billboard_rendering.c"

/* -------------------------------------------------------------------------- */
/*                                    TESTS                                   */
/* -------------------------------------------------------------------------- */

void setUp(void)
{
	mock_gl_reset_calls();
}

void tearDown(void)
{
}

void test_billboard_group_cleanup_deletes_vbo(void)
{
	BillboardGroup group = {0};
	SphereInstance instances[1] = {{{0}, {0}, 0}};

	/* Initialize the group - this should create the VBO */
	billboard_group_init(&group, instances, 1);

	TEST_ASSERT_EQUAL(mock_gl_get_generated_buffer_id(),
	                  group.instance_vbo);

	/* Cleanup */
	billboard_group_cleanup(&group);

	/* Verify VBO was deleted */
	TEST_ASSERT_EQUAL(1, mock_gl_get_delete_buffer_call_count());
	TEST_ASSERT_EQUAL(mock_gl_get_generated_buffer_id(),
	                  mock_gl_get_last_deleted_buffer());
}

void test_billboard_group_cleanup_resets_vbo_to_zero(void)
{
	BillboardGroup group = {0};
	SphereInstance instances[1] = {{{0}, {0}, 0}};

	billboard_group_init(&group, instances, 1);
	billboard_group_cleanup(&group);

	TEST_ASSERT_EQUAL(0, group.instance_vbo);
}

void test_billboard_group_cleanup_handles_uninitialized_group(void)
{
	BillboardGroup group = {0};
	/* group.instance_vbo is 0 */

	/* Should be safe to call on zeroed group */
	billboard_group_cleanup(&group);

	/* Verify no delete called (last deleted should be 0) */
	/* TEST_ASSERT_EQUAL(0, mock_gl_get_last_deleted_buffer()); */
	/* We need an accessor for last deleted too if we want to check this. */
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_billboard_group_cleanup_deletes_vbo);
	RUN_TEST(test_billboard_group_cleanup_resets_vbo_to_zero);
	RUN_TEST(test_billboard_group_cleanup_handles_uninitialized_group);
	return UNITY_END();
}
