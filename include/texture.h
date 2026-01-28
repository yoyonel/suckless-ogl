#ifndef TEXTURE_H
#define TEXTURE_H

#include "gl_common.h"

/* Load HDR texture from file */
GLuint texture_load_hdr(const char* path, int* width, int* height);

/* Upload raw HDR data to GPU */
GLuint texture_upload_hdr(float* data, int width, int height);

/* Load raw pixels from file (exposed for async loading) */
float* texture_load_pixels(const char* path, int* width, int* height,
                           int* channels);

#endif /* TEXTURE_H */
