#include "sphere_sorting.h"

#include "gl_common.h"           /* For SIMD_ALIGNMENT */
#include "instanced_rendering.h" /* For SphereInstance */
#include "log.h"
#include "utils.h"      /* For safe_memcpy */
#include <cglm/types.h> /* For vec3 */
#include <cglm/vec3.h>  /* For glm_vec3_distance2 */
#include <stdlib.h>
#include <string.h>

/*
 * Comparator for qsort.
 * Sorts descending by depth (Furthest first -> Back-to-Front).
 */
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

enum { INITIAL_CAPACITY = 64 };

void sphere_sorter_init(SphereSorter* sorter, int initial_capacity)
{
	if (initial_capacity <= 0) {
		initial_capacity = INITIAL_CAPACITY;
	}
	sorter->capacity = initial_capacity;
	sorter->entries = calloc(initial_capacity, sizeof(SphereSortEntry));

	size_t size = initial_capacity * sizeof(SphereInstance);
	if (size % SIMD_ALIGNMENT != 0) {
		size += SIMD_ALIGNMENT - (size % SIMD_ALIGNMENT);
	}
	sorter->temp_instances = aligned_alloc(SIMD_ALIGNMENT, size);

	if (!sorter->entries || !sorter->temp_instances) {
		LOG_ERROR("suckless-ogl.sphere_sorter",
		          "Failed to allocate sphere sorting buffers");
	}
}

void sphere_sorter_cleanup(SphereSorter* sorter)
{
	if (sorter->entries) {
		free(sorter->entries);
		sorter->entries = NULL;
	}
	if (sorter->temp_instances) {
		free(sorter->temp_instances);
		sorter->temp_instances = NULL;
	}
	sorter->capacity = 0;
}

void sphere_sorter_sort(SphereSorter* sorter, SphereInstance* instances,
                        int count, vec3 camera_pos)
{
	if (count <= 0 || !instances) {
		return;
	}

	/* 1. Ensure Capacity */
	if (count > sorter->capacity) {
		int new_cap = count * 2;
		SphereSortEntry* new_entries =
		    realloc(sorter->entries, new_cap * sizeof(SphereSortEntry));

		/* Free old aligned buffer and allocate new one */
		/* realloc is not guaranteed to preserve alignment > default,
		   and aligned_realloc is not standard C11 */
		free(sorter->temp_instances);
		/* Size must be multiple of alignment */
		size_t size = new_cap * sizeof(SphereInstance);
		if (size % SIMD_ALIGNMENT != 0) {
			size += SIMD_ALIGNMENT - (size % SIMD_ALIGNMENT);
		}
		SphereInstance* new_temp = aligned_alloc(SIMD_ALIGNMENT, size);

		if (!new_entries || !new_temp) {
			LOG_ERROR("suckless-ogl.sphere_sorter",
			          "Failed to resize sorting buffers to %d",
			          new_cap);
			/* Try to recover or maintain old state if possible,
			   but temp list is gone. */
			if (new_entries) {
				sorter->entries = new_entries;
			}
			/* Capacity stays same if failed? Or broken state?
			   Ideally handle better, but just bail. */
			return;
		}

		sorter->entries = new_entries;
		sorter->temp_instances = new_temp;
		sorter->capacity = new_cap;
	}

	/* 2. Compute Depths and Indices */
	for (int i = 0; i < count; ++i) {
		/* model[3] is the translation column in column-major mat4 */
		vec3 pos = {instances[i].model[3][0], instances[i].model[3][1],
		            instances[i].model[3][2]};

		sorter->entries[i].original_index = i;
		sorter->entries[i].depth = glm_vec3_distance2(pos, camera_pos);
	}

	/* 3. Sort Indices */
	qsort(sorter->entries, count, sizeof(SphereSortEntry),
	      compare_depth_desc);

	/* 4. Reorder Instances into Temp Buffer */
	for (int i = 0; i < count; ++i) {
		int old_idx = sorter->entries[i].original_index;
		/* Struct copy */
		sorter->temp_instances[i] = instances[old_idx];
	}

	/* 5. Copy back to original array */
	safe_memcpy(instances, count * sizeof(SphereInstance),
	            sorter->temp_instances, count * sizeof(SphereInstance));
}
