#include "app.h"
#include "app_settings.h"
#include "async_loader.h"
#include "ibl_coordinator.h"
#include "log.h"
#include "postprocess.h"
#include "texture.h"
#include "utils.h"
#include <dirent.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const HDR_TEXTURE_PATH = "assets/textures/hdr";
static const char* const HDR_EXTENSION = ".hdr";

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
		ibl_coordinator_start(&app->ibl_coord, hdr_tex, req->width,
		                      req->height);
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

static void finalize_ibl_swap(App* app, GLuint hdr_tex, GLuint spec_tex,
                              GLuint irr_tex, float threshold)
{
	postprocess_set_exposure(&app->postprocess, threshold);
	app->auto_threshold = threshold;

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

	app->hdr_texture = hdr_tex;
	app->spec_prefiltered_tex = spec_tex;
	app->irradiance_tex = irr_tex;

	LOG_INFO("suckless-ogl.app", "[Frame %llu] Environment swap finalized.",
	         (unsigned long long)app->frame_count);
}

static void handle_ibl_transition_done(App* app, GLuint hdr_tex,
                                       GLuint spec_tex, GLuint irr_tex,
                                       float threshold)
{
	if (app->transition_state == TRANSITION_WAIT_IBL) {
		/* Initial load: Stay black, just swap and fade in */
		finalize_ibl_swap(app, hdr_tex, spec_tex, irr_tex, threshold);
		app->transition_state = TRANSITION_FADE_IN;
		app->transition_alpha = 1.0F;
	} else if (app->transition_state == TRANSITION_LOADING) {
		/* Crossfade mode: Capture, swap and fade in */
		capture_snapshot(app);
		finalize_ibl_swap(app, hdr_tex, spec_tex, irr_tex, threshold);
		app->transition_state = TRANSITION_FADE_IN;
		app->transition_alpha = 1.0F;
	}
}

void app_process_ibl_state_machine(App* app)
{
	IBLState state =
	    ibl_coordinator_update(&app->ibl_coord, app->frame_count);

	if (state == IBL_STATE_DONE) {
		if (app->transition_state == TRANSITION_LOADING &&
		    app->env_transition_mode == ENV_TRANSITION_BLACK_SCREEN) {
			/* Just trigger fade out, don't consume yet */
			app->transition_state = TRANSITION_FADE_OUT;
			app->transition_alpha = 0.0F;
		} else if (app->transition_state == TRANSITION_WAIT_IBL ||
		           app->transition_state == TRANSITION_LOADING) {
			GLuint hdr = 0;
			GLuint spec = 0;
			GLuint irr = 0;
			float threshold = 0.0F;
			if (ibl_coordinator_get_results(&app->ibl_coord, &hdr,
			                                &spec, &irr,
			                                &threshold)) {
				handle_ibl_transition_done(app, hdr, spec, irr,
				                           threshold);
			}
		}
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
				GLuint hdr_tex = 0;
				GLuint spec_tex = 0;
				GLuint irr_tex = 0;
				float threshold = 0.0F;
				if (ibl_coordinator_get_results(
				        &app->ibl_coord, &hdr_tex, &spec_tex,
				        &irr_tex, &threshold)) {
					finalize_ibl_swap(app, hdr_tex,
					                  spec_tex, irr_tex,
					                  threshold);
					app->transition_state =
					    TRANSITION_FADE_IN;
				}
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
