/**
 * @file shader.h
 * @brief High-level OpenGL shader management with metadata and uniform caching.
 */

#ifndef SHADER_H
#define SHADER_H

#include "gl_common.h"
#include <stdbool.h>

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
 * @brief Compiles a single shader stage from a source string.
 * @param source Null-terminated shader source code.
 * @param type GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, or GL_COMPUTE_SHADER.
 * @return GLuint handle of the compiled shader stage.
 */
GLuint shader_compile_from_source(const char* source, GLenum type);

/**
 * @brief Reads a shader file into RAM, processing all includes.
 * @param path Absolute or relative path.
 * @return Heap-allocated null-terminated string. Result must be freed.
 */
char* shader_read_file(const char* path);

/**
 * @brief Reads a shader file into RAM, processing all includes.
 * @note This is an alias for shader_read_file but explicit about resolution.
 * @param path Absolute or relative path.
 * @return Heap-allocated null-terminated string. Result must be freed.
 */
char* shader_read_resolved_source(const char* path);

/**
 * @brief Injects `#define` directives into shader source.
 * @param source Original source code.
 * @param defines Array of macro strings (e.g. "BLOOM_ENABLED").
 * @param count Number of macros.
 * @return Heap-allocated string with injected defines. Result must be freed.
 */
char* shader_inject_defines(const char* source, const char** defines, int count);

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
typedef struct {
	GLuint program;        /**< OpenGL Program handle. */
	char* name;            /**< Descriptive name for debugging (owned). */
	UniformEntry* entries; /**< Sorted array of cached uniforms. */
	int entry_count;       /**< Number of active uniforms. */
	int entry_capacity;    /**< Allocation size. */
	bool
	    silent_warnings; /**< If true, missing uniforms won't log errors. */
} Shader;

/**
 * @brief Loads a linked program and caches all its active uniforms.
 * @param vertex_path Path to vertex source.
 * @param fragment_path Path to fragment source.
 * @return Pointer to the Shader object.
 */
Shader* shader_load(const char* vertex_path, const char* fragment_path);

/**
 * @brief Creates a shader object from source strings.
 * @param vert_src Vertex shader source.
 * @param frag_src Fragment shader source.
 * @param name Descriptive name for the shader.
 * @return Pointer to the Shader object.
 */
Shader* shader_create_from_source(const char* vert_src, const char* frag_src, const char* name);

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

#endif /* SHADER_H */
