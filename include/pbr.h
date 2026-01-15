#ifndef PBR_H
#define PBR_H

#include "gl_common.h"

/* Load HDR texture from file */
GLuint build_prefiltered_specular_map(GLuint env_hdr_tex, int width,
                                      int height);

GLuint build_irradiance_map(GLuint env_hdr_tex, int size, float threshold);

GLuint build_brdf_lut_map(int size);

float compute_mean_luminance_gpu(GLuint hdr_tex, int width, int height,
                                 float clamp_multiplier);

#endif /* PBR_H */
