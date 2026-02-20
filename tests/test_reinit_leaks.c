#include "instanced_rendering.h"
#include "mock_gl_standalone.h"
#include "perf_timer.h"
#include "render_utils.h"
#include "unity.h"
#include <string.h>

/* Stub for render_utils dependency */
void render_utils_setup_sphere_instance_attributes(GLsizei stride,
                                                   size_t albedo_offset,
                                                   size_t metallic_offset)
{
	(void)stride;
	(void)albedo_offset;
	(void)metallic_offset;
}

void setUp(void)
{
	mock_gl_reset_calls();
}

void tearDown(void)
{
}

void test_gpu_timer_reinit_leak(void)
{
	GPUTimer timer = {0};

	// First initialization
	gpu_timer_start(&timer);

	// gpu_timer_start calls glGenQueries twice.
	// mock implementation increments once per glGenQueries call.
	int expected_gen_calls = 2;
	TEST_ASSERT_EQUAL_MESSAGE(expected_gen_calls,
	                          mock_gl_get_gen_queries_call_count(),
	                          "First init should generate queries");

	// Call start again on the same timer without cleanup
	// This simulates a potential leak or re-use scenario.
	// If the implementation correctly handles re-init, it should clean up
	// old queries first.
	gpu_timer_start(&timer);

	// Assert that we deleted the previous queries (2 calls to
	// glDeleteQueries) If the bug exists (no cleanup), this will fail
	// (count == 0).
	TEST_ASSERT_EQUAL_MESSAGE(2, mock_gl_get_delete_queries_call_count(),
	                          "Re-init should cleanup old queries");

	// Cleanup manually at end
	gpu_timer_cleanup(&timer);
}

void test_instanced_group_reinit_leak(void)
{
	InstancedGroup group = {0};
	SphereInstance data[1];
	memset(data, 0, sizeof(data));

	// First initialization
	instanced_group_init(&group, data, 1);

	TEST_ASSERT_EQUAL_MESSAGE(1, mock_gl_get_gen_buffers_call_count(),
	                          "First init should generate buffer");

	// Call init again on same group
	instanced_group_init(&group, data, 1);

	// Assert that we deleted the previous buffer (1 call to
	// glDeleteBuffers) If the bug exists, this will fail (count == 0).
	TEST_ASSERT_EQUAL_MESSAGE(1, mock_gl_get_delete_buffer_call_count(),
	                          "Re-init should cleanup old buffer");

	instanced_group_cleanup(&group);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gpu_timer_reinit_leak);
	RUN_TEST(test_instanced_group_reinit_leak);
	return UNITY_END();
}
