/**
 * @file sphere_sorting.h
 * @brief Back-to-front sorting for transparent geometry.
 *
 * This module provides an efficient sorting mechanism for sphere instances,
 * which is required for correct alpha blending of billboarded spheres.
 */

#ifndef SPHERE_SORTING_H
#define SPHERE_SORTING_H

#include "instanced_rendering.h"
#include <cglm/cglm.h>

/**
 * @struct SphereSortEntry
 * @brief Lightweight proxy for sorting data without moving large structs.
 */
typedef struct {
	int original_index; /**< Position in the source array. */
	float depth;        /**< Squared distance from camera. */
} SphereSortEntry;

/**
 * @struct SphereSorter
 * @brief Reusable memory context for sorting operations.
 */
typedef struct {
	SphereSortEntry* entries;       /**< Key array. */
	SphereSortEntry* temp_entries;  /**< Scratchpad for Radix Sort. */
	SphereInstance* temp_instances; /**< Scratchpad for reordering. */
	int capacity;     /**< Current allocated size of temp_instances. */
	int min_capacity; /**< Minimum capacity to maintain (from
	                     initialization). */
} SphereSorter;

/**
 * @brief Allocates internal buffers for the sorter.
 * @param sorter Pointer to the struct.
 * @param initial_capacity Expected number of instances.
 */
void sphere_sorter_init(SphereSorter* sorter, int initial_capacity);

/**
 * @brief Destroys the sorter context.
 * @param sorter Pointer to the struct.
 */
void sphere_sorter_cleanup(SphereSorter* sorter);

/**
 * @brief Sorts the array of instances Back-to-Front (descending depth).
 *
 * This function Reorders the 'instances' array using a temporary
 * buffer to avoid extra copies. It swaps the pointer provided by
 * `instances_ptr` with its internal buffer.
 *
 * The provided `instances_ptr` will point to a buffer that is at least
 * as large as the `initial_capacity` passed to `sphere_sorter_init`, or
 * large enough for `count` instances, whichever is greater.
 *
 * @param sorter      Memory context.
 * @param instances_ptr Pointer to the array pointer to sort. The pointer
 * *instances_ptr will be updated to point to the sorted array.
 * @param count       Active element count.
 * @param camera_pos  World-space viewer position (for depth calculation).
 */
void sphere_sorter_sort(SphereSorter* sorter, SphereInstance** instances_ptr,
                        int count, vec3 camera_pos);

#endif /* SPHERE_SORTING_H */
