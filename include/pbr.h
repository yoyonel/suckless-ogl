/**
 * @file pbr.h
 * @brief Physically-Based Rendering (PBR) and Image-Based Lighting (IBL)
 * utilities.
 *
 * This module handles the generation of pre-filtered environment maps
 * (specular, irradiance, BRDF LUT) and GPU-accelerated luminance computation.
 */

#ifndef PBR_H
#define PBR_H

#include "gl_common.h"

/**
 * @brief Generates a pre-filtered specular environment map in a single pass.
 * @param shader Prefiltering compute shader.
 * @param env_hdr_tex Source HDR environment map.
 * @param width Destination width.
 * @param height Destination height.
 * @param threshold Luminance threshold for importance sampling.
 * @return GLuint handle of the generated cubemap.
 */
GLuint build_prefiltered_specular_map(GLuint shader, GLuint env_hdr_tex,
                                      int width, int height, float threshold);

/**
 * @brief Initializes a texture for progressive specular pre-filtering.
 * @param width Base level width.
 * @param height Base level height.
 * @return GLuint handle of the empty cubemap with allocated mips.
 */
GLuint pbr_prefilter_init(int width, int height);

/**
 * @brief Computes a single slice or mip-level for progressive pre-filtering.
 * @param shader Compute shader.
 * @param env_hdr_tex Source HDR map.
 * @param dest_tex Destination cubemap.
 * @param width Current level width.
 * @param height Current level height.
 * @param level Current mip level being processed.
 * @param total_levels Total number of mips in the cubemap.
 * @param slice_index Current face/slice being processed.
 * @param total_slices Total slices (usually 6).
 * @param threshold Luminance threshold.
 *
 * @note This function does NOT issue a memory barrier. The caller must call
 *       glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) once after all
 *       slices have been dispatched.
 */
void pbr_prefilter_mip(GLuint shader, GLuint env_hdr_tex, GLuint dest_tex,
                       int width, int height, int level, int total_levels,
                       int slice_index, int total_slices, float threshold);

/**
 * @brief Generates an irradiance map (diffuse IBL) in a single pass.
 * @param shader Irradiance convolution shader.
 * @param env_hdr_tex Source HDR map.
 * @param size Destination resolution (width/height).
 * @param threshold Luminance threshold.
 * @return GLuint handle of the irradiance cubemap.
 */
GLuint build_irradiance_map(GLuint shader, GLuint env_hdr_tex, int size,
                            float threshold);

/**
 * @brief Initializes a texture for progressive irradiance computation.
 * @param size Destination resolution.
 * @return GLuint handle of the empty cubemap.
 */
GLuint pbr_irradiance_init(int size);

/**
 * @brief Computes one face/slice of the irradiance map.
 * @param shader Compute shader.
 * @param env_hdr_tex Source HDR map.
 * @param dest_tex Destination cubemap.
 * @param size Resolution.
 * @param slice_index Current face.
 * @param total_slices Total faces.
 * @param threshold Luminance threshold.
 *
 * @note This function does NOT issue a memory barrier. The caller must call
 *       glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) once after all
 *       slices have been dispatched.
 */
void pbr_irradiance_slice_compute(GLuint shader, GLuint env_hdr_tex,
                                  GLuint dest_tex, int size, int slice_index,
                                  int total_slices, float threshold);

/**
 * @brief Generates the 2D BRDF Integration Look-Up Table.
 * @param size LUT resolution (width and height).
 * @return GLuint handle of the 2D RG texture.
 */
GLuint build_brdf_lut_map(int size);

/**
 * @brief Computes the average luminance of an HDR texture on the GPU.
 *
 * Uses a two-pass parallel reduction via compute shaders and SSBOs.
 * @param shader_pass1 First reduction pass shader.
 * @param shader_pass2 Final reduction pass shader.
 * @param hdr_tex Source HDR texture.
 * @param width Texture width.
 * @param height Texture height.
 * @param clamp_multiplier Value to clamp extreme pixels.
 * @param ssbos Pair of SSBO handles for intermediate and final results.
 * @return Average luminance value.
 */
float compute_mean_luminance_gpu(GLuint shader_pass1, GLuint shader_pass2,
                                 GLuint hdr_tex, int width, int height,
                                 float clamp_multiplier, GLuint ssbos[2]);

#endif /* PBR_H */
