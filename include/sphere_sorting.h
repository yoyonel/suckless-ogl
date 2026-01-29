#ifndef SPHERE_SORTING_H
#define SPHERE_SORTING_H

#include "instanced_rendering.h"
#include <cglm/cglm.h>

/**
 * Helper structure to store sort keys (depth) separately from data.
 * This avoids sorting large structs directly and avoids global variables for
 * qsort.
 */
typedef struct {
	int original_index;
	float depth; /* Squared distance from camera */
} SphereSortEntry;

/**
 * Manages memory for sorting operations to avoid per-frame allocations.
 */
typedef struct {
	SphereSortEntry* entries;
	SphereInstance* temp_instances;
	int capacity;
} SphereSorter;

/**
 * Initializes the sorter with an initial capacity.
 */
void sphere_sorter_init(SphereSorter* sorter, int initial_capacity);

/**
 * Frees internal buffers.
 */
void sphere_sorter_cleanup(SphereSorter* sorter);

/**
 * Sorts the array of instances Back-to-Front relative to the camera position.
 * This function Reorders the 'instances' array IN-PLACE.
 *
 * @param sorter      Context for memory reuse.
 * @param instances   The array of SphereInstance to sort.
 * @param count       Number of instances.
 * @param camera_pos  World space position of the camera.
 */
void sphere_sorter_sort(SphereSorter* sorter, SphereInstance* instances,
                        int count, vec3 camera_pos);

#endif
