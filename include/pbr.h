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
 * @brief Cached uniform locations for specular prefiltering shader.
 */
typedef struct {
	GLint u_env_map;
	GLint u_roughness;
	GLint u_mip;
	GLint u_threshold;
	GLint u_offset_y;
	GLint u_max_y;
} PBRSpecUniforms;

/**
 * @brief Cached uniform locations for radiance convolution shader.
 */
typedef struct {
	GLint u_threshold;
	GLint u_offset_y;
	GLint u_max_y;
} PBRIrrUniforms;

/**
 * @brief Cached uniform locations for luminance reduction shader.
 */
typedef struct {
	GLint u_numGroups;
	GLint u_numPixels;
} PBRLumUniforms;

/**
 * @brief Retrieves uniform locations for the specular prefilter shader.
 * @param shader The shader program.
 * @param out Pointer to the struct to populate.
 */
void pbr_get_spec_uniforms(GLuint shader, PBRSpecUniforms* out);

/**
 * @brief Retrieves uniform locations for the irradiance convolution shader.
 * @param shader The shader program.
 * @param out Pointer to the struct to populate.
 */
void pbr_get_irr_uniforms(GLuint shader, PBRIrrUniforms* out);

/**
 * @brief Retrieves uniform locations for the luminance reduction shader.
 * @param shader The shader program.
 * @param out Pointer to the struct to populate.
 */
void pbr_get_lum_uniforms(GLuint shader, PBRLumUniforms* out);

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
 * @param uniforms Cached uniform locations.
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
void pbr_prefilter_mip(GLuint shader, const PBRSpecUniforms* uniforms,
                       GLuint env_hdr_tex, GLuint dest_tex, int width,
                       int height, int level, int total_levels, int slice_index,
                       int total_slices, float threshold);

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
 * @param uniforms Cached uniform locations.
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
void pbr_irradiance_slice_compute(GLuint shader, const PBRIrrUniforms* uniforms,
                                  GLuint env_hdr_tex, GLuint dest_tex, int size,
                                  int slice_index, int total_slices,
                                  float threshold);

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
 * @param uniforms Cached uniform locations for pass 2.
 * @return Average luminance value.
 */
/**
 * @brief Starts the GPU computation for mean luminance (non-blocking).
 * Dispatches compute shaders but does NOT wait for results.
 * Caller should issue a memory barrier or fence sync after calling this.
 */
void compute_mean_luminance_gpu_start(GLuint shader_pass1, GLuint shader_pass2,
                                      GLuint hdr_tex, int width, int height,
                                      GLuint ssbos[2],
                                      const PBRLumUniforms* uniforms);

/**
 * @brief Reads the result of mean luminance computation from the SSBO.
 * This function should be called only after the GPU has finished the
 * computation started by compute_mean_luminance_gpu_start.
 * @param ssbos The SSBOs used in the computation.
 * @param clamp_multiplier Multiplier to apply to the result.
 * @return The computed mean luminance.
 */
float compute_mean_luminance_gpu_result(GLuint ssbos[2],
                                        float clamp_multiplier);

#endif /* PBR_H */
