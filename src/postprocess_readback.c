#include "app_settings.h"
#include "effects/fx_auto_exposure.h"
#include "effects/fx_motion_blur.h"
#include "gl_common.h"
#include "postprocess.h"

void postprocess_update_time(PostProcess* post_processing, float delta_time)
{
	post_processing->time += delta_time;
	post_processing->delta_time =
	    delta_time; /* Save dt for compute shader */
}

GLuint postprocess_get_exposure_pbo(PostProcess* post_processing, int index)
{
	return post_processing->readback.exposure_pbo[index];
}

GLuint postprocess_get_histogram_pbo(PostProcess* post_processing, int index)
{
	return post_processing->readback.histogram_pbo[index];
}

GLsync postprocess_get_exposure_sync(PostProcess* post_processing, int index)
{
	return post_processing->readback.exposure_sync[index];
}

GLsync postprocess_get_histogram_sync(PostProcess* post_processing, int index)
{
	return post_processing->readback.histogram_sync[index];
}

void postprocess_set_exposure_sync(PostProcess* post_processing, int index,
                                   GLsync sync)
{
	post_processing->readback.exposure_sync[index] = sync;
}

void postprocess_set_histogram_sync(PostProcess* post_processing, int index,
                                    GLsync sync)
{
	post_processing->readback.histogram_sync[index] = sync;
}

static const float LUM_MIN_EXTREME = 1e30F;
static const float LUM_MAX_EXTREME = -1e30F;

void postprocess_update_readbacks(PostProcess* post_processing,
                                  uint64_t frame_count)
{
	post_processing->readback.frame_count = frame_count;

	if (!postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE)) {
		return;
	}

	int read_idx = (int)(frame_count % 2);
	GLsync current_sync = post_processing->readback.exposure_sync[read_idx];

	if (current_sync) {
		GLenum res = glClientWaitSync(current_sync, 0, 0);
		if (res == GL_ALREADY_SIGNALED ||
		    res == GL_CONDITION_SATISFIED) {
			glBindBuffer(
			    GL_PIXEL_PACK_BUFFER,
			    post_processing->readback.exposure_pbo[read_idx]);
			float* ptr = (float*)glMapBuffer(GL_PIXEL_PACK_BUFFER,
			                                 GL_READ_ONLY);
			if (ptr) {
				post_processing->readback.current_exposure =
				    *ptr;
				glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			}
			glDeleteSync(current_sync);
			post_processing->readback.exposure_sync[read_idx] =
			    NULL;
		}
	}
}

void postprocess_set_exposure_target(PostProcess* post_processing,
                                     float threshold)
{
	post_processing->readback.auto_threshold = threshold;
	postprocess_set_exposure(post_processing, threshold);
}

static void fill_histogram_buckets(const float* lum_data, int* buckets,
                                   int size, float* min_lum, float* max_lum)
{
	for (int i = 0; i < LUM_HISTOGRAM_SIZE; i++) {
		float val = lum_data[i];
		if (val < *min_lum) {
			*min_lum = val;
		}
		if (val > *max_lum) {
			*max_lum = val;
		}

		static const float RANGE_OFFSET = 5.0F;
		static const float RANGE_SCALE = 10.0F;
		float norm = (val + RANGE_OFFSET) / RANGE_SCALE;
		int idx = (int)(norm * (float)size);
		if (idx < 0) {
			idx = 0;
		}
		if (idx >= size) {
			idx = size - 1;
		}
		buckets[idx]++;
	}
}

static void trigger_histogram_readback(PostProcess* post_processing,
                                       int write_idx)
{
	glBindTexture(GL_TEXTURE_2D,
	              post_processing->auto_exposure_fx.downsample_tex);
	glBindBuffer(GL_PIXEL_PACK_BUFFER,
	             post_processing->readback.histogram_pbo[write_idx]);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, 0);
	post_processing->readback.histogram_sync[write_idx] =
	    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

int postprocess_compute_luminance_histogram(PostProcess* post_processing,
                                            uint64_t frame_count, int* buckets,
                                            int size, float* min_lum,
                                            float* max_lum)
{
	/* Initialize buckets */
	for (int i = 0; i < size; i++) {
		buckets[i] = 0;
	}

	static const float HISTO_MIN_INIT = 1000.0F;
	static const float HISTO_MAX_INIT = -1000.0F;
	*min_lum = HISTO_MIN_INIT;
	*max_lum = HISTO_MAX_INIT;

	int read_idx = (int)(frame_count % 2);
	GLsync current_sync =
	    post_processing->readback.histogram_sync[read_idx];

	int processed = 0;
	if (current_sync) {
		GLenum res = glClientWaitSync(current_sync, 0, 0);
		if (res == GL_ALREADY_SIGNALED ||
		    res == GL_CONDITION_SATISFIED) {
			glBindBuffer(
			    GL_PIXEL_PACK_BUFFER,
			    post_processing->readback.histogram_pbo[read_idx]);
			float* lum_data = (float*)glMapBuffer(
			    GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

			if (lum_data) {
				/* Zero out cache before filling to avoid
				 * accumulation
				 */
				for (int i = 0;
				     i < POSTPROCESS_HISTOGRAM_BUCKETS; i++) {
					post_processing->readback
					    .last_buckets[i] = 0;
				}

				*min_lum = LUM_MIN_EXTREME;
				*max_lum = LUM_MAX_EXTREME;

				fill_histogram_buckets(
				    lum_data,
				    post_processing->readback.last_buckets,
				    size, min_lum, max_lum);
				post_processing->readback.last_min_lum =
				    *min_lum;
				post_processing->readback.last_max_lum =
				    *max_lum;
				post_processing->readback
				    .last_histogram_updated = 1;
				glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			}
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

			glDeleteSync(current_sync);
			post_processing->readback.histogram_sync[read_idx] =
			    NULL;
		}
	}

	/* Provide continuous data from cache if available */
	if (post_processing->readback.last_histogram_updated) {
		for (int i = 0; i < size && i < POSTPROCESS_HISTOGRAM_BUCKETS;
		     i++) {
			buckets[i] = post_processing->readback.last_buckets[i];
		}
		*min_lum = post_processing->readback.last_min_lum;
		*max_lum = post_processing->readback.last_max_lum;
		processed = 1;
	}

	/* Trigger Async Transfer for Next Slot if not already pending.
	 * This is a safety fallback for standalone tests or benchmarks.
	 */
	int write_idx = (int)((frame_count + 1) % 2);
	if (!post_processing->readback.histogram_sync[write_idx]) {
		trigger_histogram_readback(post_processing, write_idx);
	}

	return processed;
}

void postprocess_update_matrices(PostProcess* post_processing, mat4 view_proj)
{
	fx_motion_blur_update_matrices(&post_processing->motion_blur_fx,
	                               view_proj);
}
