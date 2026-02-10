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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * SLICING constants for progressive IBL loading.
 * In software-rendering mode (llvmpipe, swrast), slicing is disabled
 * (1 slice per step) to avoid the massive overhead of split dispatches.
 */
static bool is_software_renderer(void)
{
	const char* renderer = (const char*)glGetString(GL_RENDERER);
	if (!renderer) {
		return false;
	}
	return strstr(renderer, "llvmpipe") != NULL ||
	       strstr(renderer, "softpipe") != NULL ||
	       strstr(renderer, "swrast") != NULL;
}
static int ibl_irradiance_slices(void)
{
	return is_software_renderer() ? 1 : 4;
}
static int ibl_specular_mip0_slices(void)
{
	return is_software_renderer() ? 1 : 4;
}
static int ibl_specular_mip1_slices(void)
{
	return is_software_renderer() ? 1 : 2;
}
static int ibl_specular_mips_grouping_start(void)
{
	return is_software_renderer() ? 0 : 3;
}

static const int IBL_LOG_LABEL_SIZE = 128;

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
	dir_handle = opendir("assets/textures/hdr");
	if (dir_handle) {
		while ((entry = readdir(dir_handle)) != NULL) {
			char* dot = strrchr(entry->d_name, '.');
			if (dot && strcmp(dot, ".hdr") == 0) {
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
	char path
	    [256]; /* NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	            */
	if (!safe_snprintf(path, sizeof(path), "assets/textures/hdr/%s",
	                   filename)) {
		LOG_ERROR("suckless-ogl.app", "Filename too long: %s",
		          filename);
		return 0;
	}

	LOG_INFO("suckless-ogl.app", "Queuing async load for: %s", path);
	if (async_loader_request(path)) {
		app->env_map_loading = 1;
		return 1;
	}
	LOG_WARNING("suckless-ogl.app",
	            "Async load request failed/ignored for: %s", path);
	return 0;
}

void app_finalize_environment_load(App* app, AsyncRequest* req)
{
	if (!req || !req->data) {
		LOG_ERROR("suckless-ogl.app", "Async request data is NULL!");
		app->env_map_loading = 0;
		return;
	}

	LOG_INFO("suckless-ogl.app", "Finalizing environment load (GPU)...");
	GLuint hdr_tex = texture_upload_hdr(req->data, req->width, req->height);

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

			if (ctx->threshold < 1.0F || isnan(ctx->threshold) ||
			    isinf(ctx->threshold)) {
				ctx->threshold = DEFAULT_AUTO_THRESHOLD;
			}
			app->auto_threshold = ctx->threshold;
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
			ctx->state = IBL_STATE_SPECULAR_MIPS;
			break;
		}

		case IBL_STATE_SPECULAR_MIPS: {
			if (ctx->current_mip >=
			    ibl_specular_mips_grouping_start()) {
				char label[IBL_LOG_LABEL_SIZE];
				safe_snprintf(
				    label, sizeof(label),
				    "Progressive IBL: Specular Mips %d-%d",
				    ctx->current_mip, ctx->total_mips - 1);
				LOG_INFO("suckless-ogl.app",
				         "[Frame %llu] - %s...",
				         (unsigned long long)app->frame_count,
				         label);

				HYBRID_MEASURE_LOG(label)
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
				LOG_INFO("suckless-ogl.app",
				         "[Frame %llu] - %s...",
				         (unsigned long long)app->frame_count,
				         label);
				HYBRID_MEASURE_LOG(label)
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

				ctx->current_slice++;
				if (ctx->current_slice >= ctx->total_slices) {
					ctx->current_slice = 0;
					ctx->current_mip++;
				}
			}

			if (ctx->current_mip >= ctx->total_mips) {
				ctx->state = IBL_STATE_IRRADIANCE;
				ctx->current_slice = 0;
				ctx->total_slices = ibl_irradiance_slices();
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
			LOG_INFO("suckless-ogl.app", "[Frame %llu] - %s...",
			         (unsigned long long)app->frame_count, label);

			HYBRID_MEASURE_LOG(label)
			{
				pbr_irradiance_slice_compute(
				    app->shader_irmap, ctx->pending_hdr_tex,
				    ctx->pending_irr_tex, IRIDIANCE_MAP_SIZE,
				    ctx->current_slice, ctx->total_slices,
				    ctx->threshold);
			}

			ctx->current_slice++;
			if (ctx->current_slice >= ctx->total_slices) {
				ctx->state = IBL_STATE_DONE;
			}
			break;
		}

		case IBL_STATE_DONE: {
			postprocess_set_exposure(&app->postprocess,
			                         ctx->threshold);
			if (app->hdr_texture) {
				glDeleteTextures(1, &app->hdr_texture);
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

			double total_time_ms =
			    perf_timer_elapsed_ms(&ctx->global_timer);
			LOG_INFO(
			    "suckless-ogl.app",
			    "[Frame %llu] Environment updated successfully "
			    "(progressive). Total Time: %.2f ms",
			    (unsigned long long)app->frame_count,
			    total_time_ms);
			break;
		}
		default:
			break;
	}
}
