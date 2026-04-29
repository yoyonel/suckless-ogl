#ifndef PP_EXPOSURE_READBACK_H
#define PP_EXPOSURE_READBACK_H

/**
 * @file pp_exposure_readback.h
 * @brief Async GPU readback state for exposure and histogram.
 */

#include "gl_common.h"
#include <stdint.h>

#define POSTPROCESS_HISTOGRAM_BUCKETS 256

/**
 * @struct PPExposureReadback
 * @brief Async GPU readback state for exposure and histogram.
 */
typedef struct {
	GLuint
	    exposure_pbo[2]; /**< Pixel Buffer Object for mean luma readback. */
	GLuint histogram_pbo[2];  /**< Pixel Buffer Object for luminance
	                             histogram  readback. */
	GLsync exposure_sync[2];  /**< Sync objects to avoid CPU stalls on
	                             exposure  readback. */
	GLsync histogram_sync[2]; /**< Sync objects to avoid CPU stalls on
	                             histogram readback. */
	float current_exposure;   /**< Cached exposure from GPU readback. */
	float auto_threshold;     /**< Dynamic exposure target. */
	int last_buckets[POSTPROCESS_HISTOGRAM_BUCKETS];
	float last_min_lum;
	float last_max_lum;
	int last_histogram_updated;
	uint64_t frame_count; /**< Internal frame counter for readback sync. */
} PPExposureReadback;

#endif /* PP_EXPOSURE_READBACK_H */
