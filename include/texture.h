#ifndef TEXTURE_H
#define TEXTURE_H

#include "gl_common.h"

/* Load HDR texture from file */
GLuint texture_load_hdr(const char* path, int* width, int* height);

#endif /* TEXTURE_H */
