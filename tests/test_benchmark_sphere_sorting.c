#include "log.h"
#include "sphere_sorting.h"
#include <cglm/vec3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Mock logging */
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	printf("\n");
	va_end(args);
}

void log_set_callback(LogCallback callback)
{
}
void log_set_level(LogLevel level)
{
}
LogLevel log_get_level(void)
{
	return LOG_LEVEL_INFO;
}

/* Mock or helper for timing */
static double get_time_sec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static SphereInstance* generate_instances(int count)
{
	size_t size = count * sizeof(SphereInstance);
	if (size % SIMD_ALIGNMENT != 0) {
		size += SIMD_ALIGNMENT - (size % SIMD_ALIGNMENT);
	}
	SphereInstance* instances = aligned_alloc(SIMD_ALIGNMENT, size);

	for (int i = 0; i < count; ++i) {
		/* Random position in -100 to 100 range */
		float x = ((float)rand() / RAND_MAX) * 200.0f - 100.0f;
		float y = ((float)rand() / RAND_MAX) * 200.0f - 100.0f;
		float z = ((float)rand() / RAND_MAX) * 200.0f - 100.0f;

		glm_mat4_identity(instances[i].model);
		instances[i].model[3][0] = x;
		instances[i].model[3][1] = y;
		instances[i].model[3][2] = z;
	}
	return instances;
}

void verify_sort(SphereInstance* instances, int count, vec3 camera_pos)
{
	float prev_depth = 1e20f; /* Infinity */
	for (int i = 0; i < count; ++i) {
		vec3 pos = {instances[i].model[3][0], instances[i].model[3][1],
		            instances[i].model[3][2]};
		float depth = glm_vec3_distance2(pos, camera_pos);

		if (depth >
		    prev_depth + 1e-5f) { /* Tolerance for float precision */
			printf(
			    "Sort failed at index %d: depth %.6f > prev_depth "
			    "%.6f\n",
			    i, depth, prev_depth);
			exit(1);
		}
		prev_depth = depth;
	}
	printf("Sort verification passed.\n");
}

void test_benchmark_sort_performance(void)
{
	int count = 100000; /* 100k particles */
	SphereSorter sorter;
	sphere_sorter_init(&sorter, count);

	SphereInstance* instances = generate_instances(count);
	vec3 camera_pos = {0.0f, 0.0f, 0.0f};

	/* Warmup */
	sphere_sorter_sort(&sorter, &instances, count, camera_pos);

	/* Benchmark */
	double start_time = get_time_sec();
	int iterations = 50;
	for (int i = 0; i < iterations; ++i) {
		/* Slight change in camera pos to force re-sort logic if any
		 * caching (none here) */
		camera_pos[2] += 0.1f;
		sphere_sorter_sort(&sorter, &instances, count, camera_pos);
	}
	double end_time = get_time_sec();

	double avg_time = (end_time - start_time) / iterations;
	printf("Sort time for %d elements: %.6f seconds (avg over %d runs)\n",
	       count, avg_time, iterations);

	verify_sort(instances, count, camera_pos);

	sphere_sorter_cleanup(&sorter);
	free(instances);
}

int main(void)
{
	test_benchmark_sort_performance();
	return 0;
}
