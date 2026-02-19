#include "app.h"
#include "app_settings.h"
#include "async_loader.h"
#include "glad/glad.h"
#include "log.h"
#include "pbr.h"
#include "perf_timer.h"
#include "postprocess.h"
#include "texture.h"
#include "utils.h"
#include <dirent.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

static const int IBL_LOG_LABEL_SIZE = 128;
static const char* const HDR_TEXTURE_PATH = "assets/textures/hdr";
static const char* const HDR_EXTENSION = ".hdr";
static const float IBL_THRESHOLD_FALLBACK_MIN = 1.0F;

static const int MAX_PATH_LENGTH = 256;

static int compare_strings(const void* string_a, const void* string_b)
{
	return strcmp(*(const char**)string_a, *(const char**)string_b);
}

void app_scan_hdr_files(App* app)
{
	app->hdr_count = 0;
	app->hdr_files = NULL;
	app->current_hdr_index = -1;

	DIR* dir_handle = NULL;
	struct dirent* entry = NULL;
	dir_handle = opendir(HDR_TEXTURE_PATH);
	if (dir_handle) {
		while ((entry = readdir(dir_handle)) != NULL) {
			char* dot = strrchr(entry->d_name, '.');
			if (dot && strcmp(dot, HDR_EXTENSION) == 0) {
				char** new_files =
				    realloc(app->hdr_files,
				            (size_t)(app->hdr_count + 1) *
				                sizeof(char*));
				if (new_files) {
					app->hdr_files = new_files;
					app->hdr_count++;
					app->hdr_files[app->hdr_count - 1] =
					    strdup(entry->d_name);
				} else {
					LOG_ERROR(
					    "suckless-ogl.app",
					    "Failed to realloc memory for "
					    "HDR files");
				}
			}
		}
		closedir(dir_handle);

		if (app->hdr_count > 1) {
			qsort(app->hdr_files, (size_t)app->hdr_count,
			      sizeof(char*), compare_strings);
		}
	} else {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to open assets/textures/hdr directory!");
	}
	LOG_INFO("suckless-ogl.app", "Found %d HDR files.", app->hdr_count);
}

int app_load_env_map(App* app, const char* filename)
{
	if (!is_safe_filename(filename)) {
		LOG_ERROR("suckless-ogl.app",
		          "Security Violation: Invalid filename rejected: %s",
		          filename ? filename : "NULL");
		return 0;
	}

	char path[MAX_PATH_LENGTH];
	if (!safe_snprintf(path, sizeof(path), "assets/textures/hdr/%s",
	                   filename)) {
		LOG_ERROR("suckless-ogl.app", "Filename too long: %s",
		          filename);
		return false;
	}

	LOG_INFO("suckless-ogl.app", "Queuing async load for: %s", path);
	if (async_loader_request(app->async_loader, path)) {
		app->env_map_loading = 1;
		return 1;
	}
	LOG_WARNING("suckless-ogl.app",
	            "Async load request failed/ignored for: %s", path);
	return 0;
}

void app_finalize_environment_load(App* app, AsyncRequest* req)
{
	/* If half_data is NULL, we assume PBO was used. */
	if (!req || (!req->half_data && !req->pbo_mapped_ptr)) {
		/* Note: We use pbo_mapped_ptr as a flag here, even if it's
		 * logically a pointer */
		LOG_ERROR("suckless-ogl.app", "Async request data is NULL!");
		app->env_map_loading = 0;
		return;
	}

	/* 3. Upload texture to GPU (Main thread) */
	/* Note: req->half_data is already converted to FP16 by the
	 * async loader
	 */
	LOG_INFO("suckless-ogl.app", "Finalizing environment load (GPU)...");
	GLuint hdr_tex = texture_upload_hdr_from_pbo(
	    req->pbo_id, req->width, req->height, app->recycled_hdr_tex);

	/* If the upload returned a new ID (or the reused one), clear
	 * the recycled handle from App state so we don't accidentally
	 * double-free it later.
	 */
	if (hdr_tex != 0) {
		app->recycled_hdr_tex = 0;
	}

	/* 4. Free CPU memory (now that upload is done/queued) */
	if (req->half_data) {
		free(req->half_data);
		req->half_data = NULL;
	}

	if (hdr_tex) {
		app->ibl_ctx.state = IBL_STATE_LUMINANCE;
		app->ibl_ctx.pending_hdr_tex = hdr_tex;
		app->ibl_ctx.width = req->width;
		app->ibl_ctx.height = req->height;
	} else {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to create texture from HDR data!");
	}

	app->env_map_loading = 0;
}

int app_trigger_env_transition(App* app, const char* filename)
{
	/* Don't trigger if already fading or loading */
	if (app->transition_state != TRANSITION_IDLE) {
		return 0;
	}

	/* Start loading in background */
	app->transition_state = TRANSITION_LOADING;
	app->transition_alpha = 0.0F;

	return app_load_env_map(app, filename);
}

static void capture_snapshot(App* app)
{
	if (app->transition_snapshot_tex == 0) {
		glGenTextures(1, &app->transition_snapshot_tex);
	}
	glBindTexture(GL_TEXTURE_2D, app->transition_snapshot_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, app->width, app->height, 0,
	             GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, app->width,
	                    app->height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void finalize_ibl_swap(App* app)
{
	IBLContext* ctx = &app->ibl_ctx;

	postprocess_set_exposure(&app->postprocess, ctx->threshold);

	/* Recycle the old HDR texture instead of deleting it */
	if (app->hdr_texture) {
		if (app->recycled_hdr_tex) {
			/* If we already have one (weird edge case),
			 * delete the old one */
			glDeleteTextures(1, &app->recycled_hdr_tex);
		}
		app->recycled_hdr_tex = app->hdr_texture;
		/* app->hdr_texture will be overwritten below */
	}

	if (app->spec_prefiltered_tex) {
		glDeleteTextures(1, &app->spec_prefiltered_tex);
	}
	if (app->irradiance_tex) {
		glDeleteTextures(1, &app->irradiance_tex);
	}

	app->hdr_texture = ctx->pending_hdr_tex;
	app->spec_prefiltered_tex = ctx->pending_spec_tex;
	app->irradiance_tex = ctx->pending_irr_tex;

	ctx->pending_hdr_tex = 0;
	ctx->pending_spec_tex = 0;
	ctx->pending_irr_tex = 0;
	ctx->state = IBL_STATE_IDLE;

	double total_time_ms = perf_timer_elapsed_ms(&ctx->global_timer);
	LOG_INFO("suckless-ogl.app",
	         "[Frame %llu] Environment swap finalized. Total Time: "
	         "%.2f ms",
	         (unsigned long long)app->frame_count, total_time_ms);
}

/** @brief Reset stage timing statistics for a new IBL stage. */
static void ibl_stats_reset(IBLContext* ctx)
{
	perf_timer_start(&ctx->stage_timer);
	ctx->stage_gpu_min = DBL_MAX;
	ctx->stage_gpu_max = 0.0;
	ctx->stage_gpu_sum = 0.0;
	ctx->stage_slice_count = 0;
}

/** @brief Accumulate a slice's GPU timing into the stage statistics. */
static void ibl_stats_accumulate(IBLContext* ctx, double gpu_ms)
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
static void ibl_stats_log_summary(IBLContext* ctx, unsigned long long frame,
                                  const char* stage_name)
{
	if (ctx->stage_slice_count <= 0) {
		return;
	}
	double wall_ms = perf_timer_elapsed_ms(&ctx->stage_timer);
	double avg = ctx->stage_gpu_sum / ctx->stage_slice_count;
	LOG_INFO("suckless-ogl.app",
	         "[Frame %llu] Progressive IBL: %s — %d slices, "
	         "GPU min/avg/max: %.2f / %.2f / %.2f ms, "
	         "GPU total: %.2f ms, wall: %.2f ms",
	         frame, stage_name, ctx->stage_slice_count, ctx->stage_gpu_min,
	         avg, ctx->stage_gpu_max, ctx->stage_gpu_sum, wall_ms);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void app_process_ibl_state_machine(App* app)
{
	IBLContext* ctx = &app->ibl_ctx;
	if (ctx->state == IBL_STATE_IDLE) {
		return;
	}

	switch (ctx->state) {
		case IBL_STATE_LUMINANCE: {
			perf_timer_start(&ctx->global_timer);
			ibl_stats_reset(ctx);
			HYBRID_MEASURE_LOG("Progressive IBL: Luminance")
			{
				LOG_INFO("suckless-ogl.app",
				         "[Frame %llu] - Luminance...",
				         (unsigned long long)app->frame_count);
				ctx->threshold = compute_mean_luminance_gpu(
				    app->shader_lum_pass1,
				    app->shader_lum_pass2, ctx->pending_hdr_tex,
				    ctx->width, ctx->height,
				    DEFAULT_CLAMP_MULTIPLIER, app->lum_ssbo);
			}

			if (ctx->threshold < IBL_THRESHOLD_FALLBACK_MIN ||
			    isnan(ctx->threshold) || isinf(ctx->threshold)) {
				ctx->threshold = DEFAULT_AUTO_THRESHOLD;
			}
			app->auto_threshold = ctx->threshold;
			LOG_INFO("suckless-ogl.app",
			         "[Frame %llu] Progressive IBL: Luminance — "
			         "wall: %.2f ms",
			         (unsigned long long)app->frame_count,
			         perf_timer_elapsed_ms(&ctx->stage_timer));
			ctx->state = IBL_STATE_SPECULAR_INIT;
			break;
		}

		case IBL_STATE_SPECULAR_INIT: {
			LOG_INFO("suckless-ogl.app",
			         "[Frame %llu] - Specular Init...",
			         (unsigned long long)app->frame_count);
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
				LOG_DEBUG("suckless-ogl.app",
				          "[Frame %llu] - %s...",
				          (unsigned long long)app->frame_count,
				          label);

				HYBRID_MEASURE_DEBUG_MS(gpu_ms, label)
				{
					for (int mip = ctx->current_mip;
					     mip < ctx->total_mips; ++mip) {
						pbr_prefilter_mip(
						    app->shader_spmap,
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
				LOG_DEBUG("suckless-ogl.app",
				          "[Frame %llu] - %s...",
				          (unsigned long long)app->frame_count,
				          label);
				HYBRID_MEASURE_DEBUG_MS(gpu_ms, label)
				{
					pbr_prefilter_mip(
					    app->shader_spmap,
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
				ibl_stats_log_summary(
				    ctx, (unsigned long long)app->frame_count,
				    "Specular");
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
			LOG_DEBUG("suckless-ogl.app", "[Frame %llu] - %s...",
			          (unsigned long long)app->frame_count, label);

			HYBRID_MEASURE_DEBUG_MS(gpu_ms, label)
			{
				pbr_irradiance_slice_compute(
				    app->shader_irmap, ctx->pending_hdr_tex,
				    ctx->pending_irr_tex, IRIDIANCE_MAP_SIZE,
				    ctx->current_slice, ctx->total_slices,
				    ctx->threshold);
			}
			ibl_stats_accumulate(ctx, gpu_ms);

			ctx->current_slice++;
			if (ctx->current_slice >= ctx->total_slices) {
				ibl_stats_log_summary(
				    ctx, (unsigned long long)app->frame_count,
				    "Irradiance");
				ctx->state = IBL_STATE_DONE;
			}
			break;
		}

		case IBL_STATE_DONE: {
			/* Single deferred barrier for all preceding compute
			 * dispatches (specular + irradiance). Individual slices
			 * write to disjoint regions and read from the same
			 * source texture, so no inter-slice barriers were
			 * needed. This one barrier ensures all image stores
			 * are visible before the textures are sampled. */
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			if (app->transition_state == TRANSITION_WAIT_IBL) {
				/* Initial load: Stay black, just swap
				 * and fade in
				 */
				finalize_ibl_swap(app);
				app->transition_state = TRANSITION_FADE_IN;
				app->transition_alpha = 1.0F;
			} else if (app->transition_state ==
			           TRANSITION_LOADING) {
				if (app->env_transition_mode ==
				    ENV_TRANSITION_BLACK_SCREEN) {
					/* Black Screen mode: Start
					 * fading out to black */
					app->transition_state =
					    TRANSITION_FADE_OUT;
					app->transition_alpha = 0.0F;
				} else {
					/* Crossfade mode: Capture, swap
					 * and fade in */
					capture_snapshot(app);
					finalize_ibl_swap(app);
					app->transition_state =
					    TRANSITION_FADE_IN;
					app->transition_alpha = 1.0F;
				}
			}
			break;
		}
		default:
			break;
	}
}

void app_update_transition(App* app)
{
	switch (app->transition_state) {
		case TRANSITION_IDLE:
		case TRANSITION_LOADING:
		case TRANSITION_WAIT_IBL:
			break;

		case TRANSITION_FADE_OUT:
			app->transition_alpha +=
			    (float)app->delta_time / app->transition_duration;
			if (app->transition_alpha >= 1.0F) {
				app->transition_alpha = 1.0F;

				/* BLACK SCREEN SWAP HAPPENS HERE */
				finalize_ibl_swap(app);

				app->transition_state = TRANSITION_FADE_IN;
			}
			break;

		case TRANSITION_FADE_IN:
			app->transition_alpha -=
			    (float)app->delta_time / app->transition_duration;
			if (app->transition_alpha <= 0.0F) {
				app->transition_alpha = 0.0F;
				app->transition_state = TRANSITION_IDLE;
			}
			break;
	}
}
