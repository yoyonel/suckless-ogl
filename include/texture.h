#ifndef TEXTURE_H
#define TEXTURE_H

#include "gl_common.h"

/* Load HDR texture from file */
GLuint texture_load_hdr(const char* path, int* width, int* height);

/* Create an empty environment cubemap */
GLuint texture_create_env_cubemap(int size);

/* Build environment cubemap from HDR texture using compute shader */
GLuint texture_build_env_cubemap(GLuint hdr_texture, int size,
                                 GLuint compute_program);

#endif /* TEXTURE_H */
