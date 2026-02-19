#include "mock_gl_standalone.h"
#include "unity.h"

/* Include the source file directly to test static functions and internal logic
 */
#include "perf_timer.c"

void setUp(void)
{
	mock_gl_reset_calls();
}

void tearDown(void)
{
}

void test_gpu_timer_start_leak(void)
{
	GPUTimer timer = {0};

	/* First start: Should generate queries */
	gpu_timer_start(&timer);
	TEST_ASSERT_EQUAL(2, mock_gl_get_gen_queries_call_count());

	/* Second start: Should NOT generate new queries (re-use existing) */
	/* Currently, the code DOES generate new queries, so we expect this to
	 * fail initially */
	/* Or we can write the test to expect failure, or assert the leak */

	gpu_timer_start(&timer);

	/* If fixed, this should still be 2. If buggy, it will be 4. */
	/* I'll assert 2, so it fails if buggy. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    2, mock_gl_get_gen_queries_call_count(),
	    "gpu_timer_start leaked query handles by re-generating them!");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gpu_timer_start_leak);
	return UNITY_END();
}
