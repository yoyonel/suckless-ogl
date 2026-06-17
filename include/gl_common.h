/**
 * @file gl_common.h
 * @brief Common OpenGL definitions, RAII helpers, and utilities.
 *
 * This header ensures correct inclusion order for GLAD and GLFW, and
 * provides C-style RAII macros using the `__cleanup__` attribute.
 */

#ifndef GL_COMMON_H
#define GL_COMMON_H

#ifndef GL_COMMON_NO_GLAD
#include "glad/glad.h"
#else
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
/* Minimal GL types fallback */
typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef long GLsizeiptr;
typedef long GLintptr;
typedef void* GLsync;
typedef unsigned long long GLuint64;
typedef char GLchar;
typedef unsigned int GLbitfield;

#define GL_TRUE 1
#define GL_FALSE 0
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_RGBA32F 0x8814
#define GL_RGBA16F 0x881A
#define GL_RGB32F 0x8815
#define GL_RGB16F 0x881B
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT 0x1405
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_STREAM_DRAW 0x88E0
#define GL_WRITE_ONLY 0x88B9
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_NO_ERROR 0
#define GL_DEBUG_SOURCE_APPLICATION 0x824A

#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020
#define GL_MAP_WRITE_BIT 0x0010
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_TIMESTAMP 0x8E28
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_TIME_ELAPSED 0x88BF

/* Prototype declarations for mocks */
GLenum glGetError(void);
void glGenTextures(GLsizei n, GLuint* ids);
void glBindTexture(GLenum target, GLuint id);
void glPixelStorei(GLenum pname, GLint param);
void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                    GLsizei width, GLsizei height);
void glTexImage2D(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b,
                  GLenum f, GLenum ty, const void* p);
void glTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h,
                     GLenum f, GLenum ty, const void* p);
/* Prototype pour garantir la compilation de texture.c dans les tests unitaires
 */
#undef glCompressedTexSubImage2D
void glCompressedTexSubImage2D(unsigned int target, int level, int xoffset,
                               int yoffset, int width, int height,
                               unsigned int format, int imageSize,
                               const void* data);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glGenerateMipmap(GLenum target);
void glDeleteTextures(GLsizei n, const GLuint* ids);
void glActiveTexture(GLenum texture);
void glGenBuffers(GLsizei n, GLuint* buffers);
void glBindBuffer(GLenum target, GLuint buffer);
void glBufferData(GLenum target, GLsizeiptr size, const void* data,
                  GLenum usage);
void glDeleteBuffers(GLsizei n, const GLuint* buffers);
void* glMapBuffer(GLenum target, GLenum access);
void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                       GLbitfield access);
GLboolean glUnmapBuffer(GLenum target);
void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname,
                              GLint* params);
void glGenQueries(GLsizei n, GLuint* ids);
void glDeleteQueries(GLsizei n, const GLuint* ids);
void glQueryCounter(GLuint id, GLenum target);
void glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params);
void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params);
void glFlush(void);
void glFinish(void);
void glUseProgram(GLuint program);
void glPopDebugGroup(void);
void glPushDebugGroup(GLenum source, GLuint id, GLsizei length,
                      const GLchar* message);
void glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b,
                  GLboolean a);
#endif

#ifndef GL_COMMON_NO_GLFW
#include <GLFW/glfw3.h>
#endif
#include <stddef.h>
#include <stdint.h>

/** @brief Minimum number of vertex attributes guaranteed by OpenGL 3.3+. */
enum { MAX_VERTEX_ATTRIBS_BASELINE = 16 };

/** @brief Starting index for instanced vertex attributes. */
enum { INSTANCE_ATTR_START = 2 };

/** @brief Starting index for synchronization vertex attributes.
 *  Attributes 0-8 are used: 0=pos, 1=norm, 2-5=model, 6=albedo,
 *  7=pbr, 8=prev_center (motion blur). Cleanup starts at 9. */
enum { SYNC_ATTR_START = 9 };

/** @brief Number of vertices in a standard screen-filling quad (2 triangles).
 */
enum { SCREEN_QUAD_VERTEX_COUNT = 6 };

/**
 * @brief Memory alignment for SIMD/AVX (64-byte is AVX-512 safe and L1 cache
 * line aligned).
 */
enum { SIMD_ALIGNMENT = 64 };

/**
 * @brief Required alignment for UBO structs used with cglm (AVX mat4 ops).
 *
 * cglm's glm_mat4_copy uses AVX _mm256_store_ps which requires 32-byte
 * alignment. Any UBO struct containing mat4 (float[16]) fields must be
 * tagged with GL_UBO_ALIGNED to guarantee safe stack/heap allocation.
 */
enum { GL_UBO_ALIGNMENT = 32 };

/**
 * @brief Attribute to apply on UBO typedef to enforce AVX alignment.
 * Usage: } GL_UBO_ALIGNED MyUBOType;
 */
#define GL_UBO_ALIGNED __attribute__((aligned(GL_UBO_ALIGNMENT)))

/**
 * @brief Compile-time assertion that a UBO struct meets AVX alignment.
 * Place after the typedef to catch misconfigurations at build time.
 * @param type The UBO struct type name.
 */
#define GL_ASSERT_UBO_ALIGNMENT(type)                      \
	_Static_assert(_Alignof(type) >= GL_UBO_ALIGNMENT, \
	               #type " must be >= 32-byte aligned for AVX (cglm)")

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
	    __attribute__((cleanup(cleanup_gl_debug_group)))        \
	    __attribute__((unused)) = name

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
#define GL_SCOPE_USE_PROGRAM(prog)                           \
	glUseProgram(prog);                                  \
	GLuint _gl_prog_##__LINE__                           \
	    __attribute__((cleanup(cleanup_gl_use_program))) \
	    __attribute__((unused)) = prog

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

/**
 * @brief Idempotent OpenGL resource deletion macros.
 * These macros check if the resource handle is non-zero, delete the resource,
 * and set the handle to 0 to prevent double-deletion.
 */

#define GL_SAFE_DELETE_TEXTURE(tex)                  \
	do {                                         \
		if ((tex) != 0) {                    \
			glDeleteTextures(1, &(tex)); \
			(tex) = 0;                   \
		}                                    \
	} while (0)

#define GL_SAFE_DELETE_BUFFER(buf)                  \
	do {                                        \
		if ((buf) != 0) {                   \
			glDeleteBuffers(1, &(buf)); \
			(buf) = 0;                  \
		}                                   \
	} while (0)

#define GL_SAFE_DELETE_BUFFERS(count, ids)                  \
	do {                                                \
		if ((ids) != NULL) {                        \
			glDeleteBuffers(count, ids);        \
			for (int i = 0; i < (count); i++) { \
				(ids)[i] = 0;               \
			}                                   \
		}                                           \
	} while (0)

#define GL_SAFE_DELETE_VAO(vao)                          \
	do {                                             \
		if ((vao) != 0) {                        \
			glDeleteVertexArrays(1, &(vao)); \
			(vao) = 0;                       \
		}                                        \
	} while (0)

#define GL_SAFE_DELETE_VAOS(count, vaos)                    \
	do {                                                \
		if ((vaos) != NULL) {                       \
			glDeleteVertexArrays(count, vaos);  \
			for (int i = 0; i < (count); i++) { \
				(vaos)[i] = 0;              \
			}                                   \
		}                                           \
	} while (0)

#define GL_SAFE_DELETE_PROGRAM(prog)           \
	do {                                   \
		if ((prog) != 0) {             \
			glDeleteProgram(prog); \
			(prog) = 0;            \
		}                              \
	} while (0)

#define GL_SAFE_DELETE_FRAMEBUFFER(fbo)                  \
	do {                                             \
		if ((fbo) != 0) {                        \
			glDeleteFramebuffers(1, &(fbo)); \
			(fbo) = 0;                       \
		}                                        \
	} while (0)

#endif /* GL_COMMON_H */
