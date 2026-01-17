// tests/test_perf_timer.c
#include "perf_timer.h"
#include "unity.h"
#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

void test_perf_timer_module_exists(void)
{
	PerfTimer timer;
	memset(&timer, 0, sizeof(PerfTimer));
	perf_timer_start(&timer);
	TEST_PASS();
}

void test_perf_timer_start(void)
{
	PerfTimer timer;
	memset(&timer, 0, sizeof(PerfTimer));
	perf_timer_start(&timer);
	TEST_PASS();
}

void test_perf_timer_elapsed_ms(void)
{
	PerfTimer timer;
	memset(&timer, 0, sizeof(PerfTimer));
	perf_timer_start(&timer);

	// Simuler du travail
	for (volatile int i = 0; i < 10000; i++)
		;

	double elapsed = perf_timer_elapsed_ms(&timer);
	TEST_ASSERT_GREATER_OR_EQUAL(0.0, elapsed);
}

void test_perf_timer_elapsed_us(void)
{
	PerfTimer timer;
	memset(&timer, 0, sizeof(PerfTimer));
	perf_timer_start(&timer);

	double elapsed = perf_timer_elapsed_us(&timer);
	TEST_ASSERT_GREATER_OR_EQUAL(0.0, elapsed);
}

void test_perf_timer_elapsed_s(void)
{
	PerfTimer timer;
	memset(&timer, 0, sizeof(PerfTimer));
	perf_timer_start(&timer);

	double elapsed = perf_timer_elapsed_s(&timer);
	TEST_ASSERT_GREATER_OR_EQUAL(0.0, elapsed);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_perf_timer_module_exists);
	RUN_TEST(test_perf_timer_start);
	RUN_TEST(test_perf_timer_elapsed_ms);
	RUN_TEST(test_perf_timer_elapsed_us);
	RUN_TEST(test_perf_timer_elapsed_s);
	return UNITY_END();
}