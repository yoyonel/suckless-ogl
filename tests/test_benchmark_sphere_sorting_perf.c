#define _POSIX_C_SOURCE 200809L
#include "gl_common.h"
#include "instanced_rendering.h"
#include "sphere_sorting.h"
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* Baseline logic extracted from previous implementation */
static int compare_depth_desc(const void* lhs, const void* rhs)
{
	const SphereSortEntry* entry_a = (const SphereSortEntry*)lhs;
	const SphereSortEntry* entry_b = (const SphereSortEntry*)rhs;

	if (entry_a->depth < entry_b->depth) {
		return 1;
	}
	if (entry_a->depth > entry_b->depth) {
		return -1;
	}
	return 0;
}

/* Re-implementation of the baseline sort logic (memcpy) */
void sphere_sorter_sort_baseline(SphereSorter* sorter,
                                 SphereInstance* instances, int count,
                                 const vec3 camera_pos)
{
	if (count <= 0 || !instances) {
		return;
	}

	/* Ensure scratchpad capacity */
	if (count > sorter->min_capacity && count > sorter->ssbo_capacity) {
		void* new_entries = realloc(
		    sorter->entries, (size_t)count * sizeof(SphereSortEntry));
		void* new_temp =
		    realloc(sorter->temp_instances,
		            (size_t)count * sizeof(SphereInstance));
		if (new_entries) {
			sorter->entries = new_entries;
		}
		if (new_temp) {
			sorter->temp_instances = new_temp;
		}
		if (!new_entries || !new_temp) {
			return;
		}
	}

	/* Compute Depths */
	for (int i = 0; i < count; ++i) {
		float* pos = (float*)instances[i].model[3];
		sorter->entries[i].original_index = i;
		sorter->entries[i].depth =
		    glm_vec3_distance2(pos, (float*)camera_pos);
	}

	/* Sort Indices */
	qsort(sorter->entries, (size_t)count, sizeof(SphereSortEntry),
	      compare_depth_desc);

	/* Reorder to Temp */
	for (int i = 0; i < count; ++i) {
		int old_idx = sorter->entries[i].original_index;
		sorter->temp_instances[i] = instances[old_idx];
	}

	/* Copy Back (The overhead we are removing) */
	memcpy(instances, sorter->temp_instances,
	       (size_t)count * sizeof(SphereInstance));
}

int main(void)
{
	const int COUNT = 100000;
	const int ITERATIONS = 100;

	printf("Running Sphere Sorting Benchmark\n");
	printf("Instances: %d\n", COUNT);
	printf("Iterations: %d\n", ITERATIONS);

	SphereSorter sorter_baseline;
	SphereSorter sorter_optimized;

	sphere_sorter_init(&sorter_baseline, COUNT);
	sphere_sorter_init(&sorter_optimized, COUNT);

	SphereInstance* instances_baseline = NULL;
	SphereInstance* instances_optimized = NULL;

	posix_memalign((void**)&instances_baseline, SIMD_ALIGNMENT,
	               COUNT * sizeof(SphereInstance));
	posix_memalign((void**)&instances_optimized, SIMD_ALIGNMENT,
	               COUNT * sizeof(SphereInstance));

	/* Initialize data */
	for (int i = 0; i < COUNT; ++i) {
		glm_mat4_identity(instances_baseline[i].model);
		instances_baseline[i].model[3][2] =
		    (float)(rand() % 1000); /* Random Z depth */
	}
	memcpy(instances_optimized, instances_baseline,
	       COUNT * sizeof(SphereInstance));

	vec3 camera_pos = {0.0f, 0.0f, 0.0f};

	/* Measure Baseline */
	clock_t start_base = clock();
	for (int i = 0; i < ITERATIONS; ++i) {
		/* Randomize positions slightly to force sort work */
		instances_baseline[0].model[3][2] = (float)(rand() % 1000);
		sphere_sorter_sort_baseline(
		    &sorter_baseline, instances_baseline, COUNT, camera_pos);
	}
	clock_t end_base = clock();
	double time_base = (double)(end_base - start_base) / CLOCKS_PER_SEC;

	/* Measure Optimized (CPU path now available in API) */
	clock_t start_opt = clock();
	for (int i = 0; i < ITERATIONS; ++i) {
		instances_optimized[0].model[3][2] = (float)(rand() % 1000);
		sphere_sorter_sort_cpu(&sorter_optimized, instances_optimized,
		                       COUNT, camera_pos);
	}
	clock_t end_opt = clock();
	double time_opt = (double)(end_opt - start_opt) / CLOCKS_PER_SEC;

	printf("Baseline Total Time: %.4f s\n", time_base);
	printf("Optimized Total Time: %.4f s\n", time_opt);
	printf("Speedup: %.2fx\n", time_base / time_opt);
	printf("Avg Time per Frame (Baseline): %.4f ms\n",
	       (time_base / ITERATIONS) * 1000.0);
	printf("Avg Time per Frame (Optimized): %.4f ms\n",
	       (time_opt / ITERATIONS) * 1000.0);

	/* Cleanup */
	sphere_sorter_cleanup(&sorter_baseline);
	sphere_sorter_cleanup(&sorter_optimized);

	/* Free the buffers we allocated initially */
	free(instances_baseline);
	free(instances_optimized);

	return 0;
}
