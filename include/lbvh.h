/**
 * @file lbvh.h
 * @brief Morton-based Linear Bounding Volume Hierarchy (LBVH).
 */

#ifndef LBVH_H
#define LBVH_H

#include "instanced_rendering.h"
#include <cglm/cglm.h>
#include <stdint.h>

/**
 * @struct LBVHNode
 * @brief GPU-friendly LBVH node layout (128 bits).
 */
typedef struct {
	vec4 aabb_min;  /**< min.xyz, left_child_idx.w (leaf if < 0) */
	vec4 aabb_max;  /**< max.xyz, right_child_idx.w */
	int object_idx; /**< Index of the object in the sorted buffer. */
	int padding[3];
} LBVHNode;

/**
 * @struct SortProxy
 * @brief Lightweight proxy for Morton sorting.
 */
typedef struct {
	uint32_t code;
	int orig_idx;
} SortProxy;

/**
 * @struct LBVH
 * @brief Encapsulates the LBVH tree and persistent scratchpads.
 */
typedef struct {
	LBVHNode* nodes; /**< Flat array of tree nodes. */
	int node_count;  /**< Current number of nodes. */
	int capacity;    /**< Node capacity (usually 2*N - 1). */

	/* Persistent Scratchpads (Eliminate per-frame malloc) */
	uint32_t* morton_codes;         /**< Scratchpad for Morton codes. */
	SortProxy* proxies;             /**< Scratchpad for sorting proxies. */
	SphereInstance* sorted_spheres; /**< Scratchpad for sorted instances. */
	int scratch_capacity;           /**< Current capacity of scratchpads. */
} LBVH;

/**
 * @brief Initialize the LBVH structure and its scratchpads.
 * @param bvh The LBVH to initialize.
 * @param initial_object_capacity Max number of objects to support.
 * @return 1 on success, 0 on failure.
 */
int lbvh_init(LBVH* bvh, int initial_object_capacity);

/**
 * @brief Clean up LBVH resources and scratchpads.
 * @param bvh The LBVH to clean up.
 */
void lbvh_cleanup(LBVH* bvh);

/**
 * @brief Build the LBVH from a set of sphere instances.
 * @param bvh The LBVH to build.
 * @param instances Array of sphere instances.
 * @param count Number of instances.
 */
void lbvh_build(LBVH* bvh, const SphereInstance* instances, int count);

/**
 * @brief Calculate a 30-bit Morton code for a 3D point.
 */
uint32_t calculate_morton_3d(const vec3 pos, const vec3 scene_min,
                             const vec3 scene_max);

#endif /* LBVH_H */
