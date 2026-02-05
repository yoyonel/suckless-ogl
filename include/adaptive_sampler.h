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
	float timestamp; /**< Time elapsed since window start. */
	float value;     /**< Measured value (e.g. FPS). */
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
	double window_start_time; /**< Absolute time when the current window
	                             started. */
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
 */
void adaptive_sampler_add(AdaptiveSampler* sampler, float value);

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
 * @return 1 if a sample should be recorded, 0 otherwise.
 */
int adaptive_sampler_should_sample(AdaptiveSampler* sampler, float delta_time,
                                   double current_time);

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
 * @brief Resets the sampler state for a new window.
 * @param sampler Pointer to the sampler.
 * @param current_time New window start time.
 */
void adaptive_sampler_reset(AdaptiveSampler* sampler, double current_time);

/**
 * @brief Renders the sample distribution as an ASCII graph for logs/overlays.
 * @param sampler Pointer to the sampler.
 * @param buffer Output char buffer.
 * @param buffer_size Size of the output buffer.
 * @param width Character width of the timeline graph.
 * @param avg_value Comparison threshold for high/low visualization.
 */
void adaptive_sampler_ascii_plot(const AdaptiveSampler* sampler, char* buffer,
                                 size_t buffer_size, size_t width,
                                 float avg_value);

/**
 * @brief Frees all dynamic memory associated with the sampler.
 * @param sampler Pointer to the sampler.
 */
void adaptive_sampler_cleanup(AdaptiveSampler* sampler);

#endif /* ADAPTIVE_SAMPLER_H */
