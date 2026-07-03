#include "billboard_renderer.h"
#include "mock_gl_standalone.h"
#include "unity.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Mock render_utils function */
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

/* Include source directly to access internal state and static functions */
#include "billboard_renderer.c"

void setUp(void)
{
	mock_gl_reset_calls();
}

void tearDown(void)
{
}

void test_billboard_renderer_update_handles_overflow(void)
{
	BillboardRenderer renderer = {0};

	/* Initialize the renderer with capacity 2 */
	billboard_renderer_init(&renderer, 2);

	/* Reset mocks to clear the glBufferData call from init */
	mock_gl_reset_calls();

	/* Update with 4 instances - should trigger reallocation */
	billboard_renderer_update_from_buffer(&renderer,
	                                      999 /* mock src buffer ID */, 4);

	/* Verify reallocation happened */
	/* glBufferData should be called once (reallocation) */
	TEST_ASSERT_EQUAL_MESSAGE(
	    1, mock_gl_get_buffer_data_call_count(),
	    "Expected glBufferData (reallocation) to be called");

	/* Verify size */
	TEST_ASSERT_EQUAL_MESSAGE(4 * sizeof(SphereInstance),
	                          mock_gl_get_last_buffer_data_size(),
	                          "Expected buffer size to match new count");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_billboard_renderer_update_handles_overflow);
	return UNITY_END();
}
