/**
 * @file gl_common.h
 * @brief Common OpenGL definitions, RAII helpers, and utilities.
 *
 * This header ensures correct inclusion order for GLAD and GLFW, and
 * provides C-style RAII macros using the `__cleanup__` attribute.
 */

#ifndef GL_COMMON_H
#define GL_COMMON_H

/* IMPORTANT: GLAD must be included before any OpenGL headers */
#include "glad/glad.h"

/* Now we can include GLFW which may pull in OpenGL headers */
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Macro to offset a pointer by a byte amount (useful for EBO/VBO
 * offsets). */
#define BUFFER_OFFSET(offset) \
	((void*)(uintptr_t)(offset))  // NOLINT(performance-no-int-to-ptr)

/** @brief Minimum number of vertex attributes guaranteed by OpenGL 3.3+. */
enum { MAX_VERTEX_ATTRIBS_BASELINE = 16 };

/** @brief Starting index for instanced vertex attributes. */
enum { INSTANCE_ATTR_START = 2 };

/** @brief Starting index for synchronization vertex attributes (motion blur).
 */
enum { SYNC_ATTR_START = 8 };

/** @brief Number of vertices in a standard screen-filling quad (2 triangles).
 */
enum { SCREEN_QUAD_VERTEX_COUNT = 6 };

/**
 * @brief Memory alignment for SIMD/AVX (64-byte is AVX-512 safe and L1 cache
 * line aligned).
 */
enum { SIMD_ALIGNMENT = 64 };

/**
 * @brief Pushes a debug group to the OpenGL command stream for debugging tools.
 * @param name String label for the group.
 */
#define GL_DEBUG_PUSH(name) \
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name)

/** @brief Pops the current debug group. */
#define GL_DEBUG_POP() glPopDebugGroup()

/**
 * @brief RAII-style cleanup for OpenGL debug groups.
 */
static inline void cleanup_gl_debug_group(const char** dummy)
{
	(void)dummy;
	glPopDebugGroup();
}

/**
 * @brief Scoped OpenGL debug group. Automatically pops when leaving scope.
 * @param name Group label.
 */
#define GL_SCOPE_DEBUG_GROUP(name)                                  \
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name); \
	const char* _gl_dbg_##__LINE__                              \
	    __attribute__((cleanup(cleanup_gl_debug_group))) = name

/**
 * @brief RAII-style cleanup for OpenGL shader program binding.
 */
static inline void cleanup_gl_use_program(const GLuint* dummy)
{
	(void)dummy;
	glUseProgram(0);
}

/**
 * @brief Scoped shader program binding. Unbinds (glUseProgram(0)) when leaving
 * scope.
 * @param prog Shader program handle.
 */
#define GL_SCOPE_USE_PROGRAM(prog) \
	glUseProgram(prog);        \
	GLuint _gl_prog_##__LINE__ \
	    __attribute__((cleanup(cleanup_gl_use_program))) = prog

/**
 * @brief RAII-style cleanup for OpenGL textures.
 * Deletes the texture if the handle is non-zero.
 */
static inline void cleanup_gl_texture(GLuint* tex)
{
	if (tex && *tex) {
		glDeleteTextures(1, tex);
	}
}

/** @brief Attribute to automatically delete an OpenGL texture on scope exit. */
#define CLEANUP_TEXTURE __attribute__((cleanup(cleanup_gl_texture)))

#endif /* GL_COMMON_H */
