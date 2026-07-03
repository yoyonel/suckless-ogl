#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Include headers from the project */
#include "billboard_renderer.h"
#include "mock_gl_standalone.h"
#include "unity.h"

/* -------------------------------------------------------------------------- */
/*                             MOCK DEPS                                      */
/* -------------------------------------------------------------------------- */

void render_utils_setup_sphere_instance_attributes(GLuint binding_point,
                                                   GLsizei stride,
                                                   size_t offset_albedo,
                                                   size_t offset_metallic,
                                                   size_t offset_prev_center)
{
	(void)binding_point;
	(void)stride;
	(void)offset_albedo;
	(void)offset_metallic;
	(void)offset_prev_center;
}

void billboard_sorter_init(BillboardSorter* sorter, int initial_capacity)
{
	(void)sorter;
	(void)initial_capacity;
}

void billboard_sorter_cleanup(BillboardSorter* sorter)
{
	(void)sorter;
}

GLuint billboard_sorter_sort(BillboardSorter* sorter,
                             const SphereInstance* instances, int count,
                             const vec3 camera_pos, SortingMode mode)
{
	(void)sorter;
	(void)instances;
	(void)count;
	(void)camera_pos;
	(void)mode;
	return 999;
}

#ifndef SYNC_ATTR_START
#define SYNC_ATTR_START 10
#endif
#ifndef MAX_VERTEX_ATTRIBS_BASELINE
#define MAX_VERTEX_ATTRIBS_BASELINE 16
#endif

/* Include the source file directly to test it in isolation */
#include "billboard_renderer.c"

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

void test_billboard_renderer_cleanup_deletes_vbo(void)
{
	BillboardRenderer renderer = {0};

	/* Initialize - this should create the VBO */
	billboard_renderer_init(&renderer, 1);

	TEST_ASSERT_EQUAL(mock_gl_get_generated_buffer_id(),
	                  renderer.instance_vbo);

	/* Cleanup */
	billboard_renderer_cleanup(&renderer);

	/* Verify VBO was deleted */
	TEST_ASSERT_EQUAL(1, mock_gl_get_delete_buffer_call_count());
	TEST_ASSERT_EQUAL(mock_gl_get_generated_buffer_id(),
	                  mock_gl_get_last_deleted_buffer());
}

void test_billboard_renderer_cleanup_resets_vbo_to_zero(void)
{
	BillboardRenderer renderer = {0};

	billboard_renderer_init(&renderer, 1);
	billboard_renderer_cleanup(&renderer);

	TEST_ASSERT_EQUAL(0, renderer.instance_vbo);
}

void test_billboard_renderer_cleanup_handles_uninitialized(void)
{
	BillboardRenderer renderer = {0};
	/* renderer.instance_vbo is 0 */

	/* Should be safe to call on zeroed struct */
	billboard_renderer_cleanup(&renderer);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_billboard_renderer_cleanup_deletes_vbo);
	RUN_TEST(test_billboard_renderer_cleanup_resets_vbo_to_zero);
	RUN_TEST(test_billboard_renderer_cleanup_handles_uninitialized);
	return UNITY_END();
}
