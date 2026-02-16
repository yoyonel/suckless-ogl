#include "mock_gl_standalone.h"
#include "unity.h"

/* Include the source file directly to test static functions or internal state
 * if needed */
/* Also ensures we use the mock headers if include paths are set correctly */
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

	/* First call: Should generate queries */
	gpu_timer_start(&timer);

	TEST_ASSERT_EQUAL_INT(2, mock_gl_get_gen_queries_call_count());
	TEST_ASSERT_NOT_EQUAL(0, timer.query_start);
	TEST_ASSERT_NOT_EQUAL(0, timer.query_end);

	GLuint first_start = timer.query_start;
	GLuint first_end = timer.query_end;

	/* Second call: Should NOT generate new queries if fixed.
	   But currently it DOES, so we expect count to increase to 4.
	   Once fixed, this expectation should change to 2. */

	gpu_timer_start(&timer);

	/* Assert the FIX (no leak) */
	/* Expect count to remain 2 (no new queries generated) */
	TEST_ASSERT_EQUAL_INT(2, mock_gl_get_gen_queries_call_count());

	/* Also check that IDs did NOT change (reused) */
	TEST_ASSERT_EQUAL(first_start, timer.query_start);
	TEST_ASSERT_EQUAL(first_end, timer.query_end);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gpu_timer_start_leak);
	return UNITY_END();
}
