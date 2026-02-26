#include "adaptive_sampler.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const float SAMPLER_THRESHOLD = 1.0F;
static const size_t SAMPLER_CAPACITY = 100;
static const float BASE_VALUE = 60.0F;
static const float VALUE_VARIATION = 5.0F;
static const float TIMESTAMP_DIVISOR = 100.0F;
static const size_t PLOT_BUFFER_SIZE = 256;
static const size_t PLOT_WIDTH = 40;
static const int WARMUP_ITERATIONS = 100;
static const int BENCH_ITERATIONS = 200000;
static const int MODULO_DIVISOR = 10;

void setUp(void)
{
}
void tearDown(void)
{
}

void test_benchmark_ascii_plot(void)
{
	AdaptiveSampler sampler;
	// Initialize with dummy values
	adaptive_sampler_init(&sampler, SAMPLER_THRESHOLD, SAMPLER_CAPACITY,
	                      BASE_VALUE);

	// Manually populate samples to ensure we have data to plot
	// Ensure capacity
	if (sampler.capacity < SAMPLER_CAPACITY) {
		sampler.capacity = SAMPLER_CAPACITY;
		sampler.samples = (AdaptiveSampleItem*)realloc(
		    sampler.samples,
		    sizeof(AdaptiveSampleItem) * sampler.capacity);
	}
	sampler.count = SAMPLER_CAPACITY;

	for (size_t i = 0; i < sampler.count; ++i) {
		sampler.samples[i].timestamp =
		    (float)i / TIMESTAMP_DIVISOR;  // 0 to 1s
		sampler.samples[i].value =
		    BASE_VALUE + ((float)(i % MODULO_DIVISOR) -
		                  VALUE_VARIATION);  // Fluctuating FPS
	}

	char buffer[PLOT_BUFFER_SIZE];
	size_t width = PLOT_WIDTH;
	float avg = BASE_VALUE;

	// Warmup
	for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
		volatile float res = adaptive_sampler_get_average(&sampler);
		(void)res;
	}

	clock_t start = clock();
	for (int i = 0; i < BENCH_ITERATIONS; ++i) {
		volatile float res = adaptive_sampler_get_average(&sampler);
		(void)res;
	}
	clock_t end = clock();
	double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

	printf(
	    "Benchmark: adaptive_sampler_get_average %d iterations took %f "
	    "seconds\n",
	    BENCH_ITERATIONS, cpu_time_used);

	adaptive_sampler_cleanup(&sampler);
	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_benchmark_ascii_plot);
	return UNITY_END();
}
