/**
 * @file ibl_coordinator.h
 * @brief Coordinator for progressive Image-Based Lighting (IBL) generation.
 *
 * This module manages the state machine that generates Irradiance and
 * Prefiltered Specular maps from an HDR environment map over multiple frames
 * to avoid stalling the main thread.
 */

#ifndef IBL_COORDINATOR_H
#define IBL_COORDINATOR_H

#include "gl_common.h"
#include "pbr.h"
#include "perf_timer.h"

/**
 * @enum IBLState
 * @brief States for the IBL generation state machine.
 */
typedef enum {
	IBL_STATE_IDLE = 0,  /**< Application is waiting for a request. */
	IBL_STATE_LUMINANCE, /**< GPU-side analysis of HDR mean luminance. */
	IBL_STATE_SPECULAR_INIT, /**< Preparation of specular map textures. */
	IBL_STATE_SPECULAR_MIPS, /**< Sliced pre-filtering of specular levels.
	                          */
	IBL_STATE_IRRADIANCE,    /**< Sliced convolution of irradiance map. */
	IBL_STATE_DONE /**< Resource cleanup and texture activation. */
} IBLState;

/**
 * @struct IBLCoordinator
 * @brief Manages the progressive IBL generation process.
 */
typedef struct {
	/* --- State Machine --- */
	IBLState state;    /**< Current processing phase. */
	int current_mip;   /**< Mip level being computed. */
	int total_mips;    /**< Target mip count. */
	int width;         /**< Source texture width. */
	int height;        /**< Source texture height. */
	float threshold;   /**< Radiance threshold for sampling. */
	int current_slice; /**< Cubemap face/slice being processed. */
	int total_slices;  /**< Face count (typically 6). */

	/* --- Resources (Owned/Managed during process) --- */
	GLuint pending_hdr_tex;  /**< Source HDR texture handle. */
	GLuint pending_spec_tex; /**< Target specular map handle. */
	GLuint pending_irr_tex;  /**< Target irradiance map handle. */

	/* --- Dependencies (Injected) --- */
	GLuint shader_spmap;     /**< Specular pre-filter compute shader. */
	GLuint shader_irmap;     /**< Irradiance convolution compute shader. */
	GLuint shader_lum_pass1; /**< Luminance reduction pass 1. */
	GLuint shader_lum_pass2; /**< Luminance reduction pass 2. */
	GLuint lum_ssbo[2];      /**< SSBOs for luminance reduction. */

	PBRSpecUniforms
	    spec_uniforms; /**< Cached uniforms for specular shader. */
	PBRIrrUniforms
	    irr_uniforms; /**< Cached uniforms for irradiance shader. */

	/* --- Performance Metrics --- */
	PerfTimer global_timer; /**< Benchmarking for the entire process. */
	PerfTimer stage_timer;  /**< Wall-clock timer for the current stage. */
	double stage_gpu_min;   /**< Minimum GPU time across slices (ms). */
	double stage_gpu_max;   /**< Maximum GPU time across slices (ms). */
	double stage_gpu_sum;   /**< Accumulated GPU time across slices (ms). */
	int stage_slice_count;  /**< Number of slices completed in stage. */
} IBLCoordinator;

/**
 * @brief Initializes the IBL coordinator with necessary shader resources.
 * @param coord Pointer to the coordinator instance.
 * @param shader_spmap Handle to the specular pre-filter shader.
 * @param shader_irmap Handle to the irradiance convolution shader.
 * @param shader_lum_pass1 Handle to luminance pass 1 shader.
 * @param shader_lum_pass2 Handle to luminance pass 2 shader.
 */
void ibl_coordinator_init(IBLCoordinator* coord, GLuint shader_spmap,
                          GLuint shader_irmap, GLuint shader_lum_pass1,
                          GLuint shader_lum_pass2);

/**
 * @brief Cleanups any pending resources held by the coordinator.
 * @param coord Pointer to the coordinator instance.
 */
void ibl_coordinator_cleanup(IBLCoordinator* coord);

/**
 * @brief Starts the IBL generation process for a new HDR texture.
 * @param coord Pointer to the coordinator instance.
 * @param hdr_tex Handle to the source HDR texture (must be valid).
 * @param width Width of the HDR texture.
 * @param height Height of the HDR texture.
 */
void ibl_coordinator_start(IBLCoordinator* coord, GLuint hdr_tex, int width,
                           int height);

/**
 * @brief Advances the state machine by one step (slice or pass).
 * @param coord Pointer to the coordinator instance.
 * @param frame_count Current frame number (for logging purposes).
 * @return IBLState The new state after the update.
 */
IBLState ibl_coordinator_update(IBLCoordinator* coord, uint64_t frame_count);

/**
 * @brief Retrieves the results of the IBL generation.
 * @param coord Pointer to the coordinator instance.
 * @param out_hdr_tex Pointer to receive the HDR texture handle.
 * @param out_spec_tex Pointer to receive the specular texture handle.
 * @param out_irr_tex Pointer to receive the irradiance texture handle.
 * @param out_threshold Pointer to receive the computed luminance threshold.
 * @return 1 if results were retrieved (state was DONE), 0 otherwise.
 */
int ibl_coordinator_get_results(IBLCoordinator* coord, GLuint* out_hdr_tex,
                                GLuint* out_spec_tex, GLuint* out_irr_tex,
                                float* out_threshold);

/**
 * @brief Resets the coordinator to IDLE state.
 * @param coord Pointer to the coordinator instance.
 */
void ibl_coordinator_reset(IBLCoordinator* coord);

#endif /* IBL_COORDINATOR_H */
