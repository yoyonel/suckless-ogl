#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_DEFINES 32

__attribute__((noinline)) void bench_baseline(const char** defines, int count,
                                              int iterations,
                                              volatile size_t* out)
{
	size_t sum = 0;
	for (int iter = 0; iter < iterations; iter++) {
		size_t total_defines_len = 0;
		for (int i = 0; i < count; i++) {
			total_defines_len += strlen("#define ") +
			                     strlen(defines[i]) + strlen("\n");
		}
		sum += total_defines_len;
	}
	*out = sum;
}

__attribute__((noinline)) void bench_optimized(const char** defines, int count,
                                               int iterations,
                                               volatile size_t* out)
{
	size_t sum = 0;
	for (int iter = 0; iter < iterations; iter++) {
		size_t total_defines_len = 0;
		const size_t define_len = sizeof("#define ") - 1;
		const size_t nl_len = sizeof("\n") - 1;
		const size_t constant_len = define_len + nl_len;
		for (int i = 0; i < count; i++) {
			total_defines_len += constant_len + strlen(defines[i]);
		}
		sum += total_defines_len;
	}
	*out = sum;
}

int main()
{
	const char* defines[MAX_DEFINES];
	for (int i = 0; i < MAX_DEFINES; i++) {
		defines[i] =
		    "SOME_DEFINE_MACRO_LONG_NAME_TEST_VERY_LONG_NAME_SO_STRLEN_"
		    "IS_EXPENSIVE";
	}

	int count = MAX_DEFINES;
	int iterations = 10000000;
	volatile size_t result = 0;

	struct timespec start, end;

	// Baseline
	clock_gettime(CLOCK_MONOTONIC, &start);
	bench_baseline(defines, count, iterations, &result);
	clock_gettime(CLOCK_MONOTONIC, &end);
	double t_base =
	    (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

	// Optimized
	clock_gettime(CLOCK_MONOTONIC, &start);
	bench_optimized(defines, count, iterations, &result);
	clock_gettime(CLOCK_MONOTONIC, &end);
	double t_opt =
	    (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

	printf("Shader Inject Performance Benchmark\n");
	printf("===================================\n");
	printf("Iterations: %d\n", iterations);
	printf("Defines count: %d\n\n", count);

	printf("Baseline: %f s\n", t_base);
	printf("Optimized: %f s\n", t_opt);

	if (t_opt < t_base) {
		printf("Improvement: %.2f%%\n",
		       ((t_base - t_opt) / t_base) * 100.0);
	} else {
		printf(
		    "No measurable improvement in this specific test "
		    "environment (compiler likely optimized away baseline "
		    "strlen).\n");
	}

	return 0;
}
