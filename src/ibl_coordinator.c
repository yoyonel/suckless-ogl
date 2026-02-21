/**
 * @file ibl_coordinator.c
 * @brief Implementation of the IBL generation state machine.
 */

#include "ibl_coordinator.h"

#include "app_settings.h"
#include "log.h"
#include "pbr.h"
#include "utils.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Default slice count for software renderers (no slicing).
 * Slicing is disabled to avoid the massive overhead of multiple draw calls
 * on CPU-bound renderers (llvmpipe, etc.).
 */
static const int IBL_SOFTWARE_FALLBACK_SLICES = 1;

/** @brief Number of slices for irradiance convolution on hardware (GPU). */
static const int IBL_IRRADIANCE_HARDWARE_SLICES = 12;

/** @brief Number of slices for the largest specular mip (Mip 0) on hardware. */
static const int IBL_SPECULAR_MIP0_HARDWARE_SLICES = 24;

/** @brief Number of slices for the second specular mip (Mip 1) on hardware. */
static const int IBL_SPECULAR_MIP1_HARDWARE_SLICES = 8;

/** @brief Mip level from which specular mips are grouped in a single frame. */
static const int IBL_SPECULAR_MIP_GROUPING_START_MIP = 3;

/** @brief Mip level for software grouping (all mips at once). */
static const int IBL_SOFTWARE_MIP_GROUPING_START_MIP = 0;

static const float IBL_THRESHOLD_FALLBACK_MIN = 1.0F;
static const int IBL_LOG_LABEL_SIZE = 128;

/*
 * SLICING constants for progressive IBL loading.
 * In software-rendering mode (llvmpipe, swrast), slicing is disabled
 * (1 slice per step) to avoid the massive overhead of split dispatches.
 */
static bool is_software_renderer(void)
{
	const char* renderer = NULL;
	renderer = (const char*)glGetString(GL_RENDERER);
	if (!renderer) {
		return false;
	}
	return strstr(renderer, "llvmpipe") != NULL ||
	       strstr(renderer, "softpipe") != NULL ||
	       strstr(renderer, "swrast") != NULL;
}

static int ibl_irradiance_slices(void)
{
	return is_software_renderer() ? IBL_SOFTWARE_FALLBACK_SLICES
	                              : IBL_IRRADIANCE_HARDWARE_SLICES;
}
static int ibl_specular_mip0_slices(void)
{
	return is_software_renderer() ? IBL_SOFTWARE_FALLBACK_SLICES
	                              : IBL_SPECULAR_MIP0_HARDWARE_SLICES;
}
static int ibl_specular_mip1_slices(void)
{
	return is_software_renderer() ? IBL_SOFTWARE_FALLBACK_SLICES
	                              : IBL_SPECULAR_MIP1_HARDWARE_SLICES;
}
static int ibl_specular_mips_grouping_start(void)
{
	return is_software_renderer() ? IBL_SOFTWARE_MIP_GROUPING_START_MIP
	                              : IBL_SPECULAR_MIP_GROUPING_START_MIP;
}

/** @brief Reset stage timing statistics for a new IBL stage. */
static void ibl_stats_reset(IBLCoordinator* ctx)
{
	perf_timer_start(&ctx->stage_timer);
	ctx->stage_gpu_min = DBL_MAX;
	ctx->stage_gpu_max = 0.0;
	ctx->stage_gpu_sum = 0.0;
	ctx->stage_slice_count = 0;
}

/** @brief Accumulate a slice's GPU timing into the stage statistics. */
static void ibl_stats_accumulate(IBLCoordinator* ctx, double gpu_ms)
{
	if (gpu_ms < ctx->stage_gpu_min) {
		ctx->stage_gpu_min = gpu_ms;
	}
	if (gpu_ms > ctx->stage_gpu_max) {
		ctx->stage_gpu_max = gpu_ms;
	}
	ctx->stage_gpu_sum += gpu_ms;
	ctx->stage_slice_count++;
}

/** @brief Log a single INFO summary line for a completed IBL stage. */
static void ibl_stats_log_summary(IBLCoordinator* ctx, uint64_t frame,
                                  const char* stage_name)
{
	if (ctx->stage_slice_count <= 0) {
		return;
	}
	double wall_ms = perf_timer_elapsed_ms(&ctx->stage_timer);
	double avg = ctx->stage_gpu_sum / ctx->stage_slice_count;
	LOG_INFO("suckless-ogl.ibl",
	         "[Frame %llu] Progressive IBL: %s — %d slices, "
	         "GPU min/avg/max: %.2f / %.2f / %.2f ms, "
	         "GPU total: %.2f ms, wall: %.2f ms",
	         (unsigned long long)frame, stage_name, ctx->stage_slice_count,
	         ctx->stage_gpu_min, avg, ctx->stage_gpu_max,
	         ctx->stage_gpu_sum, wall_ms);
}

void ibl_coordinator_init(IBLCoordinator* coord, GLuint shader_spmap,
                          GLuint shader_irmap, GLuint shader_lum_pass1,
                          GLuint shader_lum_pass2)
{
	memset(coord, 0, sizeof(IBLCoordinator));
	coord->state = IBL_STATE_IDLE;

	coord->shader_spmap = shader_spmap;
	coord->shader_irmap = shader_irmap;
	coord->shader_lum_pass1 = shader_lum_pass1;
	coord->shader_lum_pass2 = shader_lum_pass2;
}

void ibl_coordinator_cleanup(IBLCoordinator* coord)
{
	ibl_coordinator_reset(coord);
	if (coord->lum_ssbo[0]) {
		glDeleteBuffers(2, coord->lum_ssbo);
		coord->lum_ssbo[0] = 0;
		coord->lum_ssbo[1] = 0;
	}
}

void ibl_coordinator_reset(IBLCoordinator* coord)
{
	if (coord->pending_hdr_tex) {
		glDeleteTextures(1, &coord->pending_hdr_tex);
		coord->pending_hdr_tex = 0;
	}
	if (coord->pending_spec_tex) {
		glDeleteTextures(1, &coord->pending_spec_tex);
		coord->pending_spec_tex = 0;
	}
	if (coord->pending_irr_tex) {
		glDeleteTextures(1, &coord->pending_irr_tex);
		coord->pending_irr_tex = 0;
	}
	coord->state = IBL_STATE_IDLE;
}

void ibl_coordinator_start(IBLCoordinator* coord, GLuint hdr_tex, int width,
                           int height)
{
	ibl_coordinator_reset(coord); /* Clear any previous pending work */

	/* coord takes ownership of the HDR texture handle for processing */
	coord->pending_hdr_tex = hdr_tex;
	coord->width = width;
	coord->height = height;
	coord->state = IBL_STATE_LUMINANCE;
}

IBLState ibl_coordinator_update(IBLCoordinator* ctx, uint64_t frame_count)
{
	if (ctx->state == IBL_STATE_IDLE || ctx->state == IBL_STATE_DONE) {
		return ctx->state;
	}

	unsigned long long frame = (unsigned long long)frame_count;

	switch (ctx->state) {
		case IBL_STATE_LUMINANCE: {
			perf_timer_start(&ctx->global_timer);
			ibl_stats_reset(ctx);
			HYBRID_MEASURE_LOG("Progressive IBL: Luminance")
			{
				LOG_INFO("suckless-ogl.ibl",
				         "[Frame %llu] - Luminance...", frame);
				ctx->threshold = compute_mean_luminance_gpu(
				    ctx->shader_lum_pass1,
				    ctx->shader_lum_pass2, ctx->pending_hdr_tex,
				    ctx->width, ctx->height,
				    DEFAULT_CLAMP_MULTIPLIER, ctx->lum_ssbo);
			}

			if (ctx->threshold < IBL_THRESHOLD_FALLBACK_MIN ||
			    isnan(ctx->threshold) || isinf(ctx->threshold)) {
				ctx->threshold = DEFAULT_AUTO_THRESHOLD;
			}

			LOG_INFO("suckless-ogl.ibl",
			         "[Frame %llu] Progressive IBL: Luminance — "
			         "wall: %.2f ms",
			         frame,
			         perf_timer_elapsed_ms(&ctx->stage_timer));
			ctx->state = IBL_STATE_SPECULAR_INIT;
			break;
		}

		case IBL_STATE_SPECULAR_INIT: {
			LOG_INFO("suckless-ogl.ibl",
			         "[Frame %llu] - Specular Init...", frame);
			ctx->pending_spec_tex =
			    pbr_prefilter_init(PREFILTERED_SPECULAR_MAP_SIZE,
			                       PREFILTERED_SPECULAR_MAP_SIZE);
			ctx->total_mips =
			    (int)floor(log2(PREFILTERED_SPECULAR_MAP_SIZE)) + 1;
			ctx->current_mip = 0;
			ctx->current_slice = 0;
			ibl_stats_reset(ctx);
			ctx->state = IBL_STATE_SPECULAR_MIPS;
			break;
		}

		case IBL_STATE_SPECULAR_MIPS: {
			if (ctx->current_mip >=
			    ibl_specular_mips_grouping_start()) {
				char label[IBL_LOG_LABEL_SIZE];
				safe_snprintf(label, sizeof(label),
				              "Progressive IBL: "
				              "Specular Mips %d-%d",
				              ctx->current_mip,
				              ctx->total_mips - 1);
				LOG_DEBUG("suckless-ogl.ibl",
				          "[Frame %llu] - %s...", frame, label);

				HYBRID_MEASURE_DEBUG_MS(gpu_ms, label)
				{
					for (int mip = ctx->current_mip;
					     mip < ctx->total_mips; ++mip) {
						pbr_prefilter_mip(
						    ctx->shader_spmap,
						    ctx->pending_hdr_tex,
						    ctx->pending_spec_tex,
						    PREFILTERED_SPECULAR_MAP_SIZE,
						    PREFILTERED_SPECULAR_MAP_SIZE,
						    mip, ctx->total_mips, 0, 1,
						    ctx->threshold);
					}
				}
				ibl_stats_accumulate(ctx, gpu_ms);
				ctx->current_mip = ctx->total_mips;
			} else {
				if (ctx->current_mip == 0) {
					ctx->total_slices =
					    ibl_specular_mip0_slices();
				} else if (ctx->current_mip == 1) {
					ctx->total_slices =
					    ibl_specular_mip1_slices();
				} else {
					ctx->total_slices = 1;
				}

				char label[IBL_LOG_LABEL_SIZE];
				safe_snprintf(label, sizeof(label),
				              "Progressive IBL: Specular Mip "
				              "%d Slice %d/%d",
				              ctx->current_mip,
				              ctx->current_slice + 1,
				              ctx->total_slices);
				LOG_DEBUG("suckless-ogl.ibl",
				          "[Frame %llu] - %s...", frame, label);
				HYBRID_MEASURE_DEBUG_MS(gpu_ms, label)
				{
					pbr_prefilter_mip(
					    ctx->shader_spmap,
					    ctx->pending_hdr_tex,
					    ctx->pending_spec_tex,
					    PREFILTERED_SPECULAR_MAP_SIZE,
					    PREFILTERED_SPECULAR_MAP_SIZE,
					    ctx->current_mip, ctx->total_mips,
					    ctx->current_slice,
					    ctx->total_slices, ctx->threshold);
				}
				ibl_stats_accumulate(ctx, gpu_ms);

				ctx->current_slice++;
				if (ctx->current_slice >= ctx->total_slices) {
					ctx->current_slice = 0;
					ctx->current_mip++;
				}
			}

			if (ctx->current_mip >= ctx->total_mips) {
				ibl_stats_log_summary(ctx, frame, "Specular");
				ctx->state = IBL_STATE_IRRADIANCE;
				ctx->current_slice = 0;
				ctx->total_slices = ibl_irradiance_slices();
				ibl_stats_reset(ctx);
				ctx->pending_irr_tex =
				    pbr_irradiance_init(IRIDIANCE_MAP_SIZE);
			}
			break;
		}

		case IBL_STATE_IRRADIANCE: {
			char label[IBL_LOG_LABEL_SIZE];
			safe_snprintf(label, sizeof(label),
			              "Progressive IBL: Irradiance Slice %d/%d",
			              ctx->current_slice + 1,
			              ctx->total_slices);
			LOG_DEBUG("suckless-ogl.ibl", "[Frame %llu] - %s...",
			          frame, label);

			HYBRID_MEASURE_DEBUG_MS(gpu_ms, label)
			{
				pbr_irradiance_slice_compute(
				    ctx->shader_irmap, ctx->pending_hdr_tex,
				    ctx->pending_irr_tex, IRIDIANCE_MAP_SIZE,
				    ctx->current_slice, ctx->total_slices,
				    ctx->threshold);
			}
			ibl_stats_accumulate(ctx, gpu_ms);

			ctx->current_slice++;
			if (ctx->current_slice >= ctx->total_slices) {
				ibl_stats_log_summary(ctx, frame, "Irradiance");
				ctx->state = IBL_STATE_DONE;
			}
			break;
		}

		case IBL_STATE_DONE:
			/* Barrier logic is better handled by caller or implicit
			 * in texture usage, but original code had it here. */
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			break;

		default:
			break;
	}

	return ctx->state;
}

int ibl_coordinator_get_results(IBLCoordinator* coord, GLuint* out_hdr_tex,
                                GLuint* out_spec_tex, GLuint* out_irr_tex,
                                float* out_threshold)
{
	if (coord->state != IBL_STATE_DONE) {
		return 0;
	}

	if (out_hdr_tex) {
		*out_hdr_tex = coord->pending_hdr_tex;
		coord->pending_hdr_tex = 0; /* Transfer ownership */
	}

	if (out_spec_tex) {
		*out_spec_tex = coord->pending_spec_tex;
		coord->pending_spec_tex = 0; /* Transfer ownership */
	}
	if (out_irr_tex) {
		*out_irr_tex = coord->pending_irr_tex;
		coord->pending_irr_tex = 0; /* Transfer ownership */
	}
	if (out_threshold) {
		*out_threshold = coord->threshold;
	}

	/* Job finished and results consumed. Reset to IDLE to prevent
	 * re-triggering completion logic. */
	coord->state = IBL_STATE_IDLE;

	return 1;
}
