#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include <glad/glad.h>

#include <stddef.h>

/**
 * @file render_utils.h
 * @brief Utilities for OpenGL rendering, focusing on robustness and NVIDIA
 * compatibility.
 *
 * This module encapsulates common rendering patterns, resource creation
 * helpers, and specific "hacks" or best practices required to ensure stability
 * across different GPU vendors, particularly NVIDIA. It provides tools for:
 * - Safe texture binding (handling missing textures without errors).
 * - Texture unit management (avoiding validation warnings).
 * - Standard geometry creation (Quads, Empty VAOs).
 * - Debugging helpers.
 */

// -----------------------------------------------------------------------------
// Texture Management
// -----------------------------------------------------------------------------

/**
 * @brief Safely binds a texture to a specific unit, using a fallback if needed.
 *
 * This function ensures that a valid texture is always bound to the specified
 * unit. If `texture` is 0 (invalid), it binds `fallback_tex` instead.
 * This is critical for robustness, preventing GL errors or black screens
 * when resources fail to load or are temporarily unavailable.
 *
 * @param unit The texture unit to bind to (e.g., GL_TEXTURE0).
 * @param texture The primary texture to bind.
 * @param fallback_tex The fallback texture to use if `texture` is 0.
 */
void render_utils_bind_texture_safe(GLenum unit, GLuint texture,
                                    GLuint fallback_tex);

/**
 * @brief Resets a range of texture units to a safe state.
 *
 * Binds the `fallback_tex` (usually a dummy black texture) to all texture units
 * from `start_unit` to `end_unit`.
 *
 * @note **NVIDIA Specific**: This is highly recommended before resizing
 * framebuffers or deleting textures that might be currently bound. NVIDIA
 * drivers can be very strict and may emit warnings or errors if you modify
 * a texture that is still "active" in a sampler unit, even if not currently
 * used by a shader. Resetting them ensures a clean state.
 *
 * @param start_unit The starting texture unit index (0 for GL_TEXTURE0).
 * @param end_unit The ending texture unit index (exclusive).
 * @param fallback_tex The safe texture to bind.
 */
void render_utils_reset_texture_units(int start_unit, int end_unit,
                                      GLuint fallback_tex);

// -----------------------------------------------------------------------------
// Debugging / Validation
// -----------------------------------------------------------------------------

/**
 * @brief Struct to hold GPU hardware and driver information.
 */
typedef struct {
	const char*
	    vendor; /**< Driver vendor (e.g., "Intel Open Source Group") */
	const char* renderer; /**< GPU renderer name (e.g., "Mesa Intel(R) UHD
	                         Graphics") */
	const char* version;  /**< OpenGL version string */
} GPUInfo;

/**
 * @brief Retrieves currently active GPU hardware and driver information.
 *
 * @return A GPUInfo struct populated with current context data.
 */
GPUInfo render_utils_get_gpu_info(void);

/**
 * @brief Sanitizes GPU vendor and renderer strings into a filesystem-safe
 * identifier.
 *
 * Lowercases alphanumeric characters and collapses multiple separators into a
 * single underscore.
 *
 * @param vendor The GPU vendor string.
 * @param renderer The GPU renderer string.
 * @param[out] buffer Destination buffer for the identifier.
 * @param size Size of the destination buffer.
 */
void render_utils_generate_gpu_identifier(const char* vendor,
                                          const char* renderer, char* buffer,
                                          size_t size);

/**
 * @brief Generates a filesystem-safe identifier for the *current* GPU.
 *
 * Sanitizes vendor and renderer strings (lowercase, alphanumeric, underscores).
 *
 * @param[out] buffer Destination buffer for the identifier.
 * @param size Size of the destination buffer.
 */
void render_utils_get_gpu_identifier(char* buffer, size_t size);

/**
 * @brief Sets up instance attributes for sphere rendering (Model, Albedo, PBR).
 *
 * Configures vertex attribute pointers for instanced arrays.
 * Assumes the instance VBO is already bound.
 *
 * @param stride Stride of the instance structure (sizeof(SphereInstance)).
 * @param offset_albedo Byte offset of the albedo field.
 * @param offset_metallic Byte offset of the metallic field (start of PBR
 * block).
 */
void render_utils_setup_sphere_instance_attributes(GLsizei stride,
                                                   size_t offset_albedo,
                                                   size_t offset_metallic);

#endif  // RENDER_UTILS_H
