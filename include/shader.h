#ifndef SHADER_H
#define SHADER_H

#include "gl_common.h"

/* Compile a single shader from file */
GLuint shader_compile(const char* path, GLenum type);

/* Read shader source from file (exposed for testing) */
char* shader_read_file(const char* path);

/* Create a shader program from vertex and fragment shader files */
GLuint shader_load_program(const char* vertex_path, const char* fragment_path);

/* Create a compute shader program */
GLuint shader_load_compute(const char* compute_path);

/* -------------------------------------------------------------------------
 * New Generic Shader API (with automatic Uniform Caching)
 * ------------------------------------------------------------------------- */

typedef struct {
	char* name;     /* Uniform name (owned) */
	GLint location; /* Cached OpenGL location */
} UniformEntry;

typedef struct {
	GLuint program;        /* OpenGL Program ID */
	UniformEntry* entries; /* Sorted dynamic array of uniform entries */
	int entry_count;
	int entry_capacity;
} Shader;

/* Load and link a shader program (vertex + fragment), automatically caching all
 * active uniforms. */
Shader* shader_load(const char* vertex_path, const char* fragment_path);

/* Load and link a compute shader, automatically caching all active uniforms. */
Shader* shader_load_compute_program(const char* compute_path);

/* Destroy the shader wrapper, freeing cached memory. Does NOT delete the GL
 * program if it was created externally, but DOES delete it if created via
 * shader_load. */
void shader_destroy(Shader* shader);

/* Use the shader program */
void shader_use(Shader* shader);

/* Get uniform location using cached binary search (O(log n)) */
GLint shader_get_uniform_location(Shader* shader, const char* name);

/* Convenient setters (using cached locations) */
void shader_set_int(Shader* shader, const char* name, int v);
void shader_set_float(Shader* shader, const char* name, float v);
void shader_set_vec2(Shader* shader, const char* name, const float* v);
void shader_set_vec3(Shader* shader, const char* name, const float* v);
void shader_set_vec4(Shader* shader, const char* name, const float* v);
void shader_set_mat4(Shader* shader, const char* name, const float* v);

#endif /* SHADER_H */
