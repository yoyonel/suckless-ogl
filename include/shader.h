/**
 * @file shader.h
 * @brief High-level OpenGL shader management with metadata and uniform caching.
 */

#ifndef SHADER_H
#define SHADER_H

#include "gl_common.h"
#include <stdbool.h>

enum { SHADER_WARNING_THROTTLE_LIMIT = 10 };

/**
 * @brief Compiles a single shader stage from a file.
 *
 * Supports recursive `@header` inclusion syntax.
 * @param path Path to the shader source file.
 * @param type GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, or GL_COMPUTE_SHADER.
 * @return GLuint handle of the compiled shader stage.
 */
GLuint shader_compile(const char* path, GLenum type);

/**
 * @brief Reads a shader file into RAM, processing all includes.
 * @param path Absolute or relative path.
 * @return Heap-allocated null-terminated string. Result must be freed.
 */
char* shader_read_file(const char* path);

/**
 * @brief Reads a shader file and injects defines.
 * @param path Path to source.
 * @param defines Array of macro strings.
 * @param count Number of macros.
 * @return Heap-allocated result.
 */
char* shader_read_file_with_defines(const char* path, const char** defines,
                                    int count);

/**
 * @brief Helper to load a classic Vertex+Fragment program from disk.
 * @return GLuint program handle.
 */
GLuint shader_load_program(const char* vertex_path, const char* fragment_path);

/**
 * @brief Helper to load a compute program from disk.
 * @return GLuint program handle.
 */
GLuint shader_load_compute(const char* compute_path);

/* -------------------------------------------------------------------------
 * NEW GENERIC SHADER API (with automatic Uniform Caching)
 * ------------------------------------------------------------------------- */

/**
 * @struct UniformEntry
 * @brief Cached uniform metadata for fast lookup.
 */
typedef struct {
	char* name;     /**< Identifier string (owned). */
	GLint location; /**< Bound GPU location. */
} UniformEntry;

/**
 * @struct Shader
 * @brief Wrapper for an OpenGL program with uniform caching and automatic
 * cleanup.
 */
typedef struct Shader {
	GLuint program;        /**< OpenGL Program handle. */
	char* name;            /**< Descriptive name for debugging (owned). */
	UniformEntry* entries; /**< Sorted array of cached uniforms. */
	int entry_count;       /**< Number of active uniforms. */
	int entry_capacity;    /**< Allocation size. */
	bool
	    silent_warnings; /**< If true, missing uniforms won't log errors. */
	int warning_count;   /**< Internal counter for log throttling. */
} Shader;

/**
 * @brief Loads a linked program and caches all its active uniforms.
 * @param vertex_path Path to vertex source.
 * @param fragment_path Path to fragment source.
 * @return Pointer to the Shader object.
 */
Shader* shader_load(const char* vertex_path, const char* fragment_path);

/**
 * @brief Loads a compute program and caches its uniforms.
 * @param compute_path Path to compute source.
 * @return Pointer to the Shader object.
 */
Shader* shader_load_compute_program(const char* compute_path);

/**
 * @brief Special loader that injects `#define` directives before compilation.
 * @param vertex_path Vertex source.
 * @param fragment_path Fragment source.
 * @param defines Array of macro strings (e.g. "BLOOM_ENABLED").
 * @param count Number of macros.
 * @return Specialized Shader object.
 */
Shader* shader_load_with_defines(const char* vertex_path,
                                 const char* fragment_path,
                                 const char** defines, int count);

/**
 * @brief Destroys the shader wrapper and deletes the GL program.
 * @param shader Pointer to the object to free.
 */
void shader_destroy(Shader* shader);

/**
 * @brief Activates the program for subsequent draw calls (`glUseProgram`).
 * @param shader Pointer to the wrapper.
 */
void shader_use(Shader* shader);

/**
 * @brief Retrieves a uniform location via binary search on the cache.
 * @param shader Pointer to the wrapper.
 * @param name Uniform identifier.
 * @return GL location, or -1 if not found.
 */
GLint shader_get_uniform_location(Shader* shader, const char* name);

/* --- FAST UNIFORM SETTERS (O(log N)) --- */

void shader_set_int(Shader* shader, const char* name, int val);
void shader_set_float(Shader* shader, const char* name, float val);
void shader_set_vec2(Shader* shader, const char* name, const float* val);
void shader_set_vec3(Shader* shader, const char* name, const float* val);
void shader_set_vec4(Shader* shader, const char* name, const float* val);
void shader_set_mat4(Shader* shader, const char* name, const float* val);

/* --- FASTER UNIFORM SETTERS (O(1) - Cached Location) --- */

void shader_set_int_loc(GLint loc, int val);
void shader_set_float_loc(GLint loc, float val);
void shader_set_vec2_loc(GLint loc, const float* val);
void shader_set_vec3_loc(GLint loc, const float* val);
void shader_set_vec4_loc(GLint loc, const float* val);
void shader_set_mat4_loc(GLint loc, const float* val);

/**
 * @brief Idempotent shader destruction macro.
 * Sets the pointer to NULL after calling shader_destroy.
 */
#define SHADER_SAFE_DESTROY(s)             \
	do {                               \
		if ((s) != NULL) {         \
			shader_destroy(s); \
			(s) = NULL;        \
		}                          \
	} while (0)

#endif /* SHADER_H */
