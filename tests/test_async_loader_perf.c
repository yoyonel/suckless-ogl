// tests/test_async_loader_perf.c
#define _POSIX_C_SOURCE 199309L
#include "async_loader.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unity.h>

static const long US_MULTIPLIER = 1000000L;
static const long MS_MULTIPLIER = 1000L;
static const long WARMUP_MS = 100L;
static const long MEASURE_MS = 1000L;

static void sleep_ms(long milliseconds)
{
	struct timespec req;
	req.tv_sec = milliseconds / MS_MULTIPLIER;
	req.tv_nsec =
	    (milliseconds % MS_MULTIPLIER) * MS_MULTIPLIER * MS_MULTIPLIER;
	nanosleep(&req, NULL);
}

void setUp(void)
{
	async_loader_init();
}

void tearDown(void)
{
	async_loader_shutdown();
}

static long timeval_to_us(struct timeval* time_val)
{
	return (time_val->tv_sec * US_MULTIPLIER) + time_val->tv_usec;
}

void test_idle_cpu_usage(void)
{
	struct rusage start;
	struct rusage end;

	// Warmup
	sleep_ms(WARMUP_MS);

	getrusage(RUSAGE_SELF, &start);

	// Sleep for 1 second
	sleep_ms(MEASURE_MS);

	getrusage(RUSAGE_SELF, &end);

	long user_us =
	    timeval_to_us(&end.ru_utime) - timeval_to_us(&start.ru_utime);
	long sys_us =
	    timeval_to_us(&end.ru_stime) - timeval_to_us(&start.ru_stime);
	long total_us = user_us + sys_us;

	printf("Idle CPU Usage: %ld us (User: %ld, Sys: %ld)\n", total_us,
	       user_us, sys_us);
}

void test_load_request(void)
{
	// Request a non-existent file. It should fail, but the state cycle
	// should complete. This verifies the worker thread wakes up.
	bool accepted = async_loader_request("non_existent_file.png");
	TEST_ASSERT_TRUE(accepted);

	// Poll until finished (by checking if we can submit again)
	int attempts = 0;
	bool finished = false;
	const int MAX_ATTEMPTS = 100;
	const int POLL_INTERVAL_MS = 10;

	while (attempts < MAX_ATTEMPTS) {  // Wait up to 1 second
		AsyncRequest req = {0};
		if (async_loader_poll(&req)) {
			// It actually loaded something? (Should not happen for
			// non-existent file) But if it did, it's finished.
			finished = true;
			break;
		}

		// Try to submit again. If successful, the previous one finished
		// (failed).
		if (async_loader_request("another_file.png")) {
			finished = true;
			break;
		}

		sleep_ms(POLL_INTERVAL_MS);
		attempts++;
	}

	TEST_ASSERT_TRUE_MESSAGE(finished,
	                         "Async request did not complete in time");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_idle_cpu_usage);
	RUN_TEST(test_load_request);
	return UNITY_END();
}
