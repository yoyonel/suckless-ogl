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
	GLuint compute_program;      /**< Compute shader for bitonic sort. */
	GLuint instance_ssbo;        /**< SSBO for input/reordered instances. */
	GLuint index_ssbo;           /**< SSBO for sorting proxy indices. */
	GLuint sorted_instance_ssbo; /**< SSBO for permutation result. */
	SphereSortEntry* entries;    /**< Scratchpad for CPU sorting. */
	SphereSortEntry* entries_aux;   /**< Aux scratchpad for Radix Sort. */
	SphereInstance* temp_instances; /**< Scratchpad for reordering. */
	int capacity;     /**< Current allocated size of SSBOs (GPU). */
	int cpu_capacity; /**< Current allocated size of CPU scratchpads. */
	int min_capacity; /**< Minimum capacity to maintain. */
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
 * @brief Sorts the array of instances Back-to-Front (descending depth) on GPU.
 *
 * Uploads instances to an SSBO, sorts them using a compute shader, and
 * prepares them for rendering.
 *
 * @param sorter      Memory context.
 * @param instances   Pointer to the array of instances to upload.
 * @param count       Active element count.
 * @param camera_pos  World-space viewer position.
 * @return The SSBO handle containing the sorted instances.
 */
GLuint sphere_sorter_sort_gpu(SphereSorter* sorter,
                              const SphereInstance* instances, int count,
                              const vec3 camera_pos);

/**
 * @brief Sorts the array of instances Back-to-Front (descending depth) on CPU.
 *
 * Uses qsort on the host and uploads the result to the SSBO.
 *
 * @param sorter      Memory context.
 * @param instances   Array of instances (will be copied and sorted internally).
 * @param count       Active element count.
 * @param camera_pos  World-space viewer position.
 * @return The SSBO handle containing the sorted instances.
 */
GLuint sphere_sorter_sort_cpu(SphereSorter* sorter,
                              const SphereInstance* instances, int count,
                              const vec3 camera_pos);

/**
 * @brief Sorts the array of instances Back-to-Front (descending depth) using
 * Radix Sort.
 * @param sorter      Memory context.
 * @param instances   Array of instances.
 * @param count       Active element count.
 * @param camera_pos  World-space viewer position (for depth).
 * @return The SSBO handle containing the sorted instances.
 */
GLuint sphere_sorter_sort_cpu_radix(SphereSorter* sorter,
                                    const SphereInstance* instances, int count,
                                    const vec3 camera_pos);

#endif /* SPHERE_SORTING_H */
