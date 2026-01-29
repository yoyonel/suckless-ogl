#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include <glad/glad.h>

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
 * @brief Creates a 1x1 floating-point texture of a specific color.
 *
 * Useful for creating "dummy" textures (e.g., black, white, normal) to bind
 * to shader slots when a real texture is not available. This prevents
 * undefined behavior or warnings from validation layers on some drivers
 * (like NVIDIA) which check that all active samplers have valid textures.
 *
 * @param r Red component (0.0 - 1.0).
 * @param g Green component (0.0 - 1.0).
 * @param b Blue component (0.0 - 1.0).
 * @param a Alpha component (0.0 - 1.0).
 * @return The GLuint handle of the created texture.
 */
GLuint render_utils_create_color_texture(float red, float green, float blue,
                                         float alpha);

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
// Geometry Helpers
// -----------------------------------------------------------------------------

/**
 * @brief Creates an empty Vertex Array Object (VAO).
 *
 * Some OpenGL profiles (Core Profile) require a VAO to be bound for *any*
 * draw call, even if the shader generates vertices without buffers (e.g.,
 * a full-screen triangle generated from gl_VertexID).
 *
 * @param[out] vao Pointer to store the generated VAO handle.
 */
void render_utils_create_empty_vao(GLuint* vao);

/**
 * @brief Creates a standard centered Quad VBO.
 *
 * Generates a VBO containing vertices for a quad centered at (0,0) with
 * size 1x1 (-0.5 to 0.5). Useful for billboard rendering or simple sprites.
 *
 * Format: 3 floats (x, y, z) per vertex.
 * Vertex count: 6 (2 triangles).
 *
 * @param[out] vbo Pointer to store the generated VBO handle.
 */
void render_utils_create_quad_vbo(GLuint* vbo);

/**
 * @brief Creates a Full-Screen Quad VAO and VBO.
 *
 * Generates a quad covering the entire Normalized Device Coordinates (NDC)
 * space (-1.0 to 1.0). Used primarily for post-processing passes.
 *
 * Includes:
 * - Positions (2 floats: x, y)
 * - Texture Coordinates (2 floats: u, v)
 *
 * @param[out] vao Pointer to store the generated VAO handle.
 * @param[out] vbo Pointer to store the generated VBO handle.
 */
void render_utils_create_fullscreen_quad(GLuint* vao, GLuint* vbo);

// -----------------------------------------------------------------------------
// Debugging / Validation
// -----------------------------------------------------------------------------

/**
 * @brief Checks the completeness of the currently bound framebuffer.
 *
 * Logs an error message if the framebuffer is not complete.
 *
 * @param label A string label to identify the framebuffer in logs.
 * @return 1 if complete, 0 otherwise.
 */
int render_utils_check_framebuffer(const char* label);

#endif  // RENDER_UTILS_H
