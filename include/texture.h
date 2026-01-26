#ifndef TEXTURE_H
#define TEXTURE_H

#include "gl_common.h"

/* Load HDR texture from file */
GLuint texture_load_hdr(const char* path, int* width, int* height);

/* Split functions for async loading */
float* texture_load_pixels(const char* path, int* width, int* height,
                           int* channels);
GLuint texture_upload_hdr(float* data, int width, int height);

#endif /* TEXTURE_H */
