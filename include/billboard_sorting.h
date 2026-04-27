/**
 * @file billboard_sorting.h
 * @brief Back-to-front sorting for transparent geometry.
 *
 * This module provides an efficient sorting mechanism for sphere instances,
 * which is required for correct alpha blending of billboarded spheres.
 */

#ifndef BILLBOARD_SORTING_H
#define BILLBOARD_SORTING_H

#include "sphere_types.h"
#include <cglm/cglm.h>

/**
 * @struct BillboardSortEntry
 * @brief Lightweight proxy for sorting data without moving large structs.
 */
typedef struct {
	int original_index; /**< Position in the source array. */
	float depth;        /**< Squared distance from camera. */
} BillboardSortEntry;

/**
 * @struct BillboardSorter
 * @brief Reusable memory context for sorting operations.
 */
typedef struct {
	GLuint compute_program;      /**< Compute shader for bitonic sort. */
	GLuint instance_ssbo;        /**< SSBO for input/reordered instances. */
	GLuint index_ssbo;           /**< SSBO for sorting proxy indices. */
	GLuint sorted_instance_ssbo; /**< SSBO for permutation result. */

	/* Cached uniform locations (resolved once at init). */
	GLint loc_stage;     /**< u_stage uniform. */
	GLint loc_count;     /**< u_count uniform. */
	GLint loc_count_pot; /**< u_count_pot uniform. */
	GLint loc_cam;       /**< u_cam_pos uniform. */
	GLint loc_j;         /**< u_j uniform. */
	GLint loc_k;         /**< u_k uniform. */

	BillboardSortEntry* entries;     /**< Scratchpad for CPU sorting. */
	BillboardSortEntry* entries_aux; /**< Aux scratchpad for Radix Sort. */
	SphereInstance* temp_instances;  /**< Scratchpad for reordering. */
	int ssbo_capacity; /**< Current allocated size of SSBOs (GPU). */
	int cpu_capacity;  /**< Current allocated size of CPU scratchpads. */
	int min_capacity;  /**< Minimum capacity to maintain. */
} BillboardSorter;

/**
 * @brief Allocates internal buffers for the sorter.
 * @param sorter Pointer to the struct.
 * @param initial_capacity Expected number of instances.
 */
void billboard_sorter_init(BillboardSorter* sorter, int initial_capacity);

/**
 * @brief Destroys the sorter context.
 * @param sorter Pointer to the struct.
 */
void billboard_sorter_cleanup(BillboardSorter* sorter);

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
GLuint billboard_sorter_sort_gpu(BillboardSorter* sorter,
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
GLuint billboard_sorter_sort_cpu(BillboardSorter* sorter,
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
GLuint billboard_sorter_sort_cpu_radix(BillboardSorter* sorter,
                                       const SphereInstance* instances,
                                       int count, const vec3 camera_pos);

#endif /* BILLBOARD_SORTING_H */
