// tests/test_perf_timer.c
#include "perf_timer.h"
#include "unity.h"
#include <string.h>

static const int LOOP_ITERATIONS = 10000;
static const double ZERO_THRESHOLD = 0.0;

void setUp(void)
{
}
void tearDown(void)
{
}

void test_perf_timer_module_exists(void)
{
	PerfTimer timer;
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.bzero)
	(void)memset(&timer, 0, sizeof(PerfTimer));
	perf_timer_start(&timer);
	TEST_PASS();
}

void test_perf_timer_start(void)
{
	PerfTimer timer;
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.bzero)
	(void)memset(&timer, 0, sizeof(PerfTimer));
	perf_timer_start(&timer);
	TEST_PASS();
}

void test_perf_timer_elapsed_ms(void)
{
	PerfTimer timer;
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.bzero)
	(void)memset(&timer, 0, sizeof(PerfTimer));
	perf_timer_start(&timer);

	// Simuler du travail
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	for (volatile int i = 0; i < LOOP_ITERATIONS; i++)
		;

	double elapsed = perf_timer_elapsed_ms(&timer);
	TEST_ASSERT_GREATER_OR_EQUAL(ZERO_THRESHOLD, elapsed);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_perf_timer_module_exists);
	RUN_TEST(test_perf_timer_start);
	RUN_TEST(test_perf_timer_elapsed_ms);
	return UNITY_END();
}
