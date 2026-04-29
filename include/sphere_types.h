/**
 * @file sphere_types.h
 * @brief Pure-data type for per-instance sphere attributes.
 *
 * This header defines the canonical SphereInstance layout shared by the
 * instanced renderer, billboard renderer, N-body simulation, light probes,
 * and sorting subsystems.  It depends only on cglm types and the
 * SIMD_ALIGNMENT constant from gl_common.h — no OpenGL entry points are
 * referenced directly.
 */

#ifndef SPHERE_TYPES_H
#define SPHERE_TYPES_H

#include "gl_common.h"
#include <cglm/cglm.h>

/**
 * @struct SphereInstance
 * @brief Per-instance data sent to the shader via an instanced VBO.
 *
 * This structure is 64-byte aligned to ensure optimal GPU throughput
 * and compatibility with SIMD-based sorting or physics.
 */
typedef struct SphereInstance {
	mat4 model;       /**< 4x4 Transformation matrix. */
	vec3 albedo;      /**< Base color (linear RGB). */
	float metallic;   /**< PBR metallic factor (0.0 - 1.0). */
	float roughness;  /**< PBR roughness factor (0.0 - 1.0). */
	float ao;         /**< Ambient occlusion factor. */
	float padding;    /**< Alignment padding. */
	vec3 prev_center; /**< Previous frame center (for per-object motion
	                     blur). */
} __attribute__((aligned(SIMD_ALIGNMENT))) SphereInstance;

#endif /* SPHERE_TYPES_H */
