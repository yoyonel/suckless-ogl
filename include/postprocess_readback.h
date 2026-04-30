/**
 * @file postprocess_readback.h
 * @brief Async GPU readback and histogram API for post-processing.
 *
 * Consumers that need PBO readback, histogram computation, or
 * matrix updates should include this header.
 */

#ifndef POSTPROCESS_READBACK_H
#define POSTPROCESS_READBACK_H

#include <glad/glad.h>

#include <cglm/types.h>
#include <stdint.h>

typedef struct PostProcess PostProcess;

/**
 * @brief Increments internal clocks.
 * @param post_processing Pointer to the struct.
 * @param delta_time SECONDS elapsed since last frame.
 */
void postprocess_update_time(PostProcess* post_processing, float delta_time);

GLuint postprocess_get_exposure_pbo(PostProcess* post_processing, int index);
GLuint postprocess_get_histogram_pbo(PostProcess* post_processing, int index);
GLsync postprocess_get_exposure_sync(PostProcess* post_processing, int index);
GLsync postprocess_get_histogram_sync(PostProcess* post_processing, int index);
void postprocess_set_exposure_sync(PostProcess* post_processing, int index,
                                   GLsync sync);
void postprocess_set_histogram_sync(PostProcess* post_processing, int index,
                                    GLsync sync);

/**
 * @brief Updates all async GPU readbacks (Exposure, Histogram).
 * Handles PBO mapping and Sync management internally to avoid CPU stalls.
 */
void postprocess_update_readbacks(PostProcess* post_processing,
                                  uint64_t frame_count);

/**
 * @brief Updates the target exposure threshold for AE.
 */
void postprocess_set_exposure_target(PostProcess* post_processing,
                                     float threshold);

/**
 * @brief Updates view-projection matrices for effects requiring
 * depth-reconstruction.
 * @param post_processing Pointer to the struct.
 * @param view_proj The current frame's View-Proj matrix.
 */
void postprocess_update_matrices(PostProcess* post_processing, mat4 view_proj);

/**
 * @brief Computes the luminance histogram from the GPU readback.
 * @return 1 if buckets were updated, 0 otherwise.
 */
int postprocess_compute_luminance_histogram(PostProcess* post_processing,
                                            uint64_t frame_count, int* buckets,
                                            int size, float* min_lum,
                                            float* max_lum);

#endif /* POSTPROCESS_READBACK_H */
