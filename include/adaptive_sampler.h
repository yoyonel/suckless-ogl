/**
 * @file adaptive_sampler.h
 * @brief Adaptive sampling utility for performance metrics.
 *
 * This module provides a mechanism to sample values (like FPS) adaptively
 * over a time window, using a PRNG to avoid aliasing artifacts.
 */

#ifndef ADAPTIVE_SAMPLER_H
#define ADAPTIVE_SAMPLER_H

#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t

/**
 * @struct AdaptiveSampleItem
 * @brief Represents a single data point in the sampler.
 */
typedef struct {
	float timestamp;      /**< Time elapsed since window start. */
	float value;          /**< Measured value (e.g. FPS). */
	uint64_t frame_index; /**< Frame index when sampled. */
} AdaptiveSampleItem;

/**
 * @struct Pcg32
 * @brief State for a Permuted Congruential Generator (PCG) RNG.
 */
typedef struct {
	uint64_t state;
	uint64_t inc;
} Pcg32;

/**
 * @struct AdaptiveSampler
 * @brief Collector for performance samples within a rolling or fixed window.
 */
typedef struct {
	/* RNG State */
	Pcg32 rng;

	/* Configuration */
	float
	    window_duration; /**< Duration of the sampling window in seconds. */
	size_t target_samples; /**< Expected number of samples to take per
	                          window. */

	/* State */
	size_t samples_taken;
	double window_start_time;    /**< Absolute time when the current window
	                                started. */
	uint64_t window_start_frame; /**< Frame index when window started. */
	uint64_t window_end_frame;   /**< Frame index of most recent sample. */
	float avg_dt; /**< Exponential Moving Average of frame deltas. */
	float alpha;  /**< EMA smoothing factor. */

	/* Buffer (Dynamic) */
	AdaptiveSampleItem* samples; /**< Allocated buffer for sample items. */
	size_t capacity;             /**< Current allocation size. */
	size_t count;                /**< Number of samples currently stored. */

} AdaptiveSampler;

/**
 * @brief Manually adds a sample to the sampler (e.g. from external source like
 * GPU profiler).
 * @param sampler Pointer to the sampler.
 * @param value The value to add.
 * @param frame_index The frame index associated with this value.
 */
void adaptive_sampler_add(AdaptiveSampler* sampler, float value,
                          uint64_t frame_index);

/**
 * @brief Initializes the adaptive sampler.
 * @param sampler Pointer to the sampler.
 * @param window_duration Length of the window in seconds.
 * @param target_samples Approximate number of samples to collect.
 * @param initial_fps_guess Starting value for EMA to avoid cold-start bias.
 */
void adaptive_sampler_init(AdaptiveSampler* sampler, float window_duration,
                           size_t target_samples, float initial_fps_guess);

/**
 * @brief Determines if a sample should be taken this frame based on
 * probability.
 * @param sampler Pointer to the sampler.
 * @param delta_time Current frame duration.
 * @param current_time Absolute time in seconds.
 * @param frame_index Current frame index.
 * @return 1 if a sample should be recorded, 0 otherwise.
 */
int adaptive_sampler_should_sample(AdaptiveSampler* sampler, float delta_time,
                                   double current_time, uint64_t frame_index);

/**
 * @brief Checks if the current sampling window has concluded.
 * @param sampler Pointer to the sampler.
 * @param current_time Absolute time in seconds.
 * @return 1 if the window duration has elapsed, 0 otherwise.
 */
int adaptive_sampler_is_finished(const AdaptiveSampler* sampler,
                                 double current_time);

/**
 * @brief Calculates the arithmetic mean of all samples in the current window.
 * @param sampler Pointer to the sampler.
 * @return The average value.
 */
float adaptive_sampler_get_average(const AdaptiveSampler* sampler);

/**
 * @brief Returns the number of samples currently in the buffer.
 * @param sampler Pointer to the sampler.
 * @return Sample count.
 */
size_t adaptive_sampler_get_sample_count(const AdaptiveSampler* sampler);

/**
 * @brief Retrieves the frame index range covered by the current window.
 * @param sampler Pointer to the sampler.
 * @param start_frame Output for start frame index (can be NULL).
 * @param end_frame Output for end frame index (can be NULL).
 */
void adaptive_sampler_get_window_range(const AdaptiveSampler* sampler,
                                       uint64_t* start_frame,
                                       uint64_t* end_frame);

/**
 * @brief Retrieves the list of frame indices for all collected samples.
 * @param sampler Pointer to the sampler.
 * @param out_indices Buffer to store the frame indices.
 * @param max_count Size of the output buffer.
 * @return Number of indices written to the buffer.
 */
size_t adaptive_sampler_get_sample_indices(const AdaptiveSampler* sampler,
                                           uint64_t* out_indices,
                                           size_t max_count);

/**
 * @brief Resets the sampler state for a new window.
 * @param sampler Pointer to the sampler.
 * @param current_time New window start time.
 */
void adaptive_sampler_reset(AdaptiveSampler* sampler, double current_time);

/**
 * @brief Frees all dynamic memory associated with the sampler.
 * @param sampler Pointer to the sampler.
 */
void adaptive_sampler_cleanup(AdaptiveSampler* sampler);

#endif /* ADAPTIVE_SAMPLER_H */
