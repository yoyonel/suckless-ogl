#ifndef GL_COMMON_H
#define GL_COMMON_H

/* IMPORTANT: GLAD must be included before any OpenGL headers */
#include "glad/glad.h"

/* Now we can include GLFW which may pull in OpenGL headers */
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <stdint.h>

#define BUFFER_OFFSET(offset) \
	((void*)(uintptr_t)(offset))  // NOLINT(performance-no-int-to-ptr)

#define SCREEN_QUAD_VERTEX_COUNT 6

/* Memory alignment for SIMD/AVX (64-byte is AVX-512 safe and L1 cache line
 * aligned) */
#define SIMD_ALIGNMENT 64

/* Debug Markers for ApiTrace / RenderDoc */
#define GL_DEBUG_PUSH(name) \
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name)
#define GL_DEBUG_POP() glPopDebugGroup()

/**
 * @brief RAII-style cleanup for OpenGL debug groups
 */
static inline void cleanup_gl_debug_group(const char** dummy)
{
	(void)dummy;
	glPopDebugGroup();
}

#define GL_SCOPE_DEBUG_GROUP(name)                                  \
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name); \
	const char* _gl_dbg_##__LINE__                              \
	    __attribute__((cleanup(cleanup_gl_debug_group))) = name

/**
 * @brief RAII-style cleanup for OpenGL shader program binding
 */
static inline void cleanup_gl_use_program(const GLuint* dummy)
{
	(void)dummy;
	glUseProgram(0);
}

#define GL_SCOPE_USE_PROGRAM(prog) \
	glUseProgram(prog);        \
	GLuint _gl_prog_##__LINE__ \
	    __attribute__((cleanup(cleanup_gl_use_program))) = prog

/**
 * @brief RAII-style cleanup for OpenGL textures
 */
static inline void cleanup_gl_texture(GLuint* tex)
{
	if (tex && *tex) {
		glDeleteTextures(1, tex);
	}
}

#define CLEANUP_TEXTURE __attribute__((cleanup(cleanup_gl_texture)))

#endif /* GL_COMMON_H */
