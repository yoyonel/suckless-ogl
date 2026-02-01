#include "async_loader.h"
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <unity.h>

void setUp(void)
{
	async_loader_init();
}

void tearDown(void)
{
	async_loader_shutdown();
}

static long timeval_to_us(struct timeval* tv)
{
	return tv->tv_sec * 1000000L + tv->tv_usec;
}

void test_idle_cpu_usage(void)
{
	struct rusage start, end;

	// Warmup
	usleep(100000);  // 100ms

	getrusage(RUSAGE_SELF, &start);

	// Sleep for 1 second
	usleep(1000000);

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

	while (attempts < 100) {  // Wait up to 1 second
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

		usleep(10000);
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
