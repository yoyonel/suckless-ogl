#include "billboard_sorting.h"
#include "mock_gl_standalone.h"  // For mock_gl_reset_calls if needed
#include <cglm/vec3.h>           // For vec3
#include <stdio.h>
#include <stdlib.h>

enum { TEST_INITIAL_CAPACITY = 1024 };
enum { TEST_COUNT = 1000 };

int main()
{
	printf("Starting Sphere Sorting Benchmark...\n");

	BillboardSorter sorter;
	billboard_sorter_init(&sorter, TEST_INITIAL_CAPACITY);

	int count = TEST_COUNT;
	SphereInstance* instances = calloc(count, sizeof(SphereInstance));
	// Fill with dummy data
	for (int i = 0; i < count; ++i) {
		instances[i].model[3][0] = (float)i;
		instances[i].model[3][1] = 0.0f;
		instances[i].model[3][2] = 0.0f;
	}

	vec3 camera_pos = {0.0f, 0.0f, 0.0f};
	GLuint ssbo =
	    billboard_sorter_sort_gpu(&sorter, instances, count, camera_pos);

	printf("Sort dispatched. SSBO: %u\n", ssbo);

	billboard_sorter_cleanup(&sorter);
	free(instances);

	printf("Done.\n");
	return 0;
}
