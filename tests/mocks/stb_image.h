#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STBI_INCLUDE_STB_IMAGE_H

#include <stdio.h>

extern int stbi_info_from_file(FILE const *f, int *x, int *y, int *comp);
extern unsigned char *stbi_load_from_file(FILE const *f, int *x, int *y, int *comp, int req_comp);
extern void stbi_image_free(void *retval_from_stbi_load);
extern float *stbi_loadf_from_file(FILE const *f, int *x, int *y, int *comp, int req_comp);

#endif
