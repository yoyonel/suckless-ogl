#ifndef PBR_H
#define PBR_H

#include "gl_common.h"

/* Prefiltered Specular Map Generation */
GLuint build_prefiltered_specular_map(GLuint shader, GLuint env_hdr_tex,
                                      int width, int height, float threshold);

/* Granular Prefiltering (for async/progressive loading) */
GLuint pbr_prefilter_init(int width, int height);
void pbr_prefilter_mip(GLuint shader, GLuint env_hdr_tex, GLuint dest_tex,
                       int width, int height, int level, int total_levels,
                       int slice_index, int total_slices, float threshold);

GLuint build_irradiance_map(GLuint shader, GLuint env_hdr_tex, int size,
                            float threshold);

GLuint pbr_irradiance_init(int size);
void pbr_irradiance_slice_compute(GLuint shader, GLuint env_hdr_tex,
                                  GLuint dest_tex, int size, int slice_index,
                                  int total_slices, float threshold);

GLuint build_brdf_lut_map(int size);

float compute_mean_luminance_gpu(GLuint shader_pass1, GLuint shader_pass2,
                                 GLuint hdr_tex, int width, int height,
                                 float clamp_multiplier);

#endif /* PBR_H */
