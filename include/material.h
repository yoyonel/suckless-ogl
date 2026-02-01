/**
 * @file material.h
 * @brief PBR material definitions and management.
 */

#ifndef MATERIAL_H
#define MATERIAL_H

#include "gl_common.h"
#include <cglm/cglm.h>

/** @brief Maximum length for a material identifier string. */
#define MAX_MATERIAL_NAME_LENGTH 64

/**
 * @struct PBRMaterial
 * @brief Surface properties for the physically-based rendering pipeline.
 *
 * This struct is aligned to SIMD boundaries for efficient library management.
 */
typedef struct {
	char name[MAX_MATERIAL_NAME_LENGTH]; /**< Unique identifier. */
	vec3 albedo;                         /**< Base color (linear RGB). */
	float
	    metallic; /**< Metalness factor (0.0 = dielectric, 1.0 = metal). */
	float roughness; /**< Surface roughness (0.0 = smooth, 1.0 = rough). */
} __attribute__((aligned(SIMD_ALIGNMENT))) PBRMaterial;

/**
 * @struct MaterialLib
 * @brief A collection of material presets loaded from disk or defined at
 * runtime.
 */
typedef struct {
	PBRMaterial* materials; /**< Array of materials. */
	int count;              /**< Number of materials in the library. */
} MaterialLib;

/**
 * @brief Loads a set of material presets from a configuration file.
 * @param path Path to the material description file.
 * @return Pointer to the allocated library, or NULL on failure.
 */
MaterialLib* material_load_presets(const char* path);

/**
 * @brief Frees all memory associated with a material library.
 * @param lib Pointer to the library to destroy.
 */
void material_free_lib(MaterialLib* lib);

#endif /* MATERIAL_H */
