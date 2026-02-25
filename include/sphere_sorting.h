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
 * LBVHNode representing a bounding volume hierarchy node.
 * GPU-friendly layout (128 bytes total if aligned).
 */
typedef struct {
	vec4 aabb_min;  /**< min.xyz, left_child.w */
	vec4 aabb_max;  /**< max.xyz, right_child.w (or -1 if leaf) */
	int object_idx; /**< Index of the sphere in the sorted instance buffer.
	                 */
	int padding[3];
} LBVHNode;

typedef struct {
	LBVHNode* nodes; /**< Flat array of tree nodes (2*N - 1 capacity). */
	int node_count;  /**< Actual number of used nodes. */
	int capacity;    /**< Max nodes allocated. */
} LBVH;
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

	/* Cached uniform locations (resolved once at init). */
	GLint loc_stage;     /**< u_stage uniform. */
	GLint loc_count;     /**< u_count uniform. */
	GLint loc_count_pot; /**< u_count_pot uniform. */
	GLint loc_cam;       /**< u_cam_pos uniform. */
	GLint loc_j;         /**< u_j uniform. */
	GLint loc_k;         /**< u_k uniform. */

	SphereSortEntry* entries;       /**< Scratchpad for CPU sorting. */
	SphereSortEntry* entries_aux;   /**< Aux scratchpad for Radix Sort. */
	SphereInstance* temp_instances; /**< Scratchpad for reordering. */
	int ssbo_capacity; /**< Current allocated size of SSBOs (GPU). */
	int cpu_capacity;  /**< Current allocated size of CPU scratchpads. */
	int min_capacity;  /**< Minimum capacity to maintain. */
} SphereSorter;

/**
 * @brief Initialize a LBVH structure.
 * @param bvh The LBVH to initialize.
 * @param initial_capacity Minimum number of nodes to support (should be 2*N-1).
 * @return 1 on success, 0 on failure.
 */
int lbvh_init(LBVH* bvh, int initial_capacity);

/**
 * @brief Clean up LBVH resources.
 * @param bvh The LBVH to clean up.
 */
void lbvh_cleanup(LBVH* bvh);

/**
 * @brief Build a Morton-based Linear BVH from a sorted set of spheres.
 * @param bvh The LBVH to build into.
 * @param instances Array of sphere instances (must be sorted by Morton code).
 * @param count Number of spheres.
 */
void lbvh_build(LBVH* bvh, SphereInstance* instances, int count);

/**
 * @brief Calculate a 30-bit Morton code for a 3D point.
 */
uint32_t calculate_morton_3d(const vec3 pos, const vec3 scene_min,
                             const vec3 scene_max);

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
