/**
 * @file icosphere.h
 * @brief Procedural icosphere geometry generation.
 *
 * This module provides utilities for generating smooth, subdivided spheres
 * starting from an icosahedron. It includes dynamic arrays for internal
 * geometry processing.
 */

#ifndef ICOSPHERE_H
#define ICOSPHERE_H

#include <cglm/cglm.h>
#include <stddef.h>

/**
 * @struct Vec3Array
 * @brief Dynamic array for cglm vec3 types.
 */
typedef struct {
	vec3* data;      /**< Pointer to the heap-allocated buffer. */
	size_t size;     /**< Number of active elements. */
	size_t capacity; /**< Total allocated space. */
} Vec3Array;

/**
 * @struct UintArray
 * @brief Dynamic array for unsigned integers (mostly for mesh indices).
 */
typedef struct {
	unsigned int* data; /**< Pointer to the heap-allocated buffer. */
	size_t size;        /**< Number of active elements. */
	size_t capacity;    /**< Total allocated space. */
} UintArray;

/**
 * @struct IcosphereGeometry
 * @brief Container for procedural mesh data.
 */
typedef struct {
	Vec3Array vertices; /**< Vertex positions. */
	Vec3Array normals;  /**< Per-vertex normals. */
	UintArray indices;  /**< Triangle indices. */
} IcosphereGeometry;

/* Array operations */

/** @brief Initializes a Vec3Array with zeroed fields. */
void vec3array_init(Vec3Array* array);
/** @brief Appends a vec3 to the array, reallocating if necessary. */
void vec3array_push(Vec3Array* array, vec3 vertex);
/** @brief Frees the internal buffer of a Vec3Array. */
void vec3array_free(Vec3Array* array);

/** @brief Initializes a UintArray with zeroed fields. */
void uintarray_init(UintArray* array);
/** @brief Appends an unsigned int to the array, reallocating if necessary. */
void uintarray_push(UintArray* array, unsigned int value);
/** @brief Frees the internal buffer of a UintArray. */
void uintarray_free(UintArray* array);

/* Icosphere operations */

/**
 * @brief Initializes icosphere geometry buffers.
 * @param geom Pointer to the geometry container.
 */
void icosphere_init(IcosphereGeometry* geom);

/**
 * @brief Generates icosphere mesh data for a specified subdivision level.
 * @param geom Pointer to the geometry container.
 * @param subdivisions Number of times to split each triangle face (0-6).
 * @see app_settings.h (MAX_SUBDIV)
 */
void icosphere_generate(IcosphereGeometry* geom, int subdivisions);

/**
 * @brief Frees all dynamic memory in the icosphere container.
 * @param geom Pointer to the geometry container.
 */
void icosphere_free(IcosphereGeometry* geom);

#endif /* ICOSPHERE_H */
