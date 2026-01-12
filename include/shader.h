#ifndef SHADER_H
#define SHADER_H

#include "gl_common.h"

/* Compile a single shader from file */
GLuint shader_compile(const char* path, GLenum type);

/* Create a shader program from vertex and fragment shader files */
GLuint shader_load_program(const char* vertex_path, const char* fragment_path);

/* Create a compute shader program */
GLuint shader_load_compute(const char* compute_path);

#endif /* SHADER_H */
