#include "app.h"
#include "app_settings.h"
#include "async_loader.h"
#include "ibl_coordinator.h"
#include "log.h"
#include "postprocess.h"
#include "texture.h"
#include "utils.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int MAX_PATH_LENGTH = 256;

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
		app->env_mgr.env_map_loading = 1;
		return 1;
	}
	LOG_WARNING("suckless-ogl.app",
	            "Async load request failed/ignored for: %s", path);
	return 0;
}

void app_process_env_map_loading_step(App* app)
{
	if (app->env_mgr.env_map_loading_step == 0) {
		return;
	}

	AsyncRequest* req = &app->env_mgr.current_env_req;

	if (app->env_mgr.env_map_loading_step == 1) {
		/* Step 1: Upload texture to GPU (No Mipmaps) */
		if (!req->half_data && !req->pbo_mapped_ptr) {
			LOG_ERROR("suckless-ogl.app",
			          "Async request data is NULL!");
			app->env_mgr.env_map_loading = 0;
			app->env_mgr.env_map_loading_step = 0;
			return;
		}

		LOG_INFO("suckless-ogl.app",
		         "Finalizing environment load (Step 1/3): Upload...");
		app->env_mgr.pending_env_tex = texture_upload_hdr_from_pbo(
		    req->pbo_id, req->width, req->height,
		    app->scene.recycled_hdr_tex);

		if (app->env_mgr.pending_env_tex != 0) {
			app->scene.recycled_hdr_tex = 0;
		}

		if (req->half_data) {
			free(req->half_data);
			req->half_data = NULL;
		}

		app->env_mgr.env_map_loading_step = 2; /* Next frame */

	} else if (app->env_mgr.env_map_loading_step == 2) {
		/* Step 2: Generate Mipmaps */
		LOG_INFO("suckless-ogl.app",
		         "Finalizing environment load (Step 2/3): Mipmaps...");
		if (app->env_mgr.pending_env_tex) {
			texture_generate_hdr_mipmap(
			    app->env_mgr.pending_env_tex);
			app->env_mgr.env_map_loading_step = 3; /* Next frame */
		} else {
			LOG_ERROR("suckless-ogl.app",
			          "Failed to create texture from HDR data!");
			app->env_mgr.env_map_loading = 0;
			app->env_mgr.env_map_loading_step = 0;
		}

	} else if (app->env_mgr.env_map_loading_step == 3) {
		/* Step 3: Start IBL Coordinator */
		LOG_INFO(
		    "suckless-ogl.app",
		    "Finalizing environment load (Step 3/3): Start IBL...");
		if (app->env_mgr.pending_env_tex) {
			ibl_coordinator_start(&app->scene.ibl_coord,
			                      app->env_mgr.pending_env_tex,
			                      req->width, req->height);
			app->env_mgr.pending_env_tex = 0;
		}
		app->env_mgr.env_map_loading = 0;
		app->env_mgr.env_map_loading_step = 0;
	}
}

int app_trigger_env_transition(App* app, const char* filename)
{
	/* Don't trigger if already fading or loading */
	if (app->env_mgr.transition_state != TRANSITION_IDLE) {
		return 0;
	}

	/* Start loading in background */
	app->env_mgr.transition_state = TRANSITION_LOADING;
	app->env_mgr.transition_alpha = 0.0F;

	return app_load_env_map(app, filename);
}

static void capture_snapshot(App* app)
{
	if (app->scene.transition_snapshot_tex == 0) {
		glGenTextures(1, &app->scene.transition_snapshot_tex);
	}
	glBindTexture(GL_TEXTURE_2D, app->scene.transition_snapshot_tex);
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
	if (app->scene.hdr_texture) {
		if (app->scene.recycled_hdr_tex) {
			/* If we already have one (weird edge case),
			 * delete the old one */
			glDeleteTextures(1, &app->scene.recycled_hdr_tex);
		}
		app->scene.recycled_hdr_tex = app->scene.hdr_texture;
		/* app->scene.hdr_texture will be overwritten below */
	}

	if (app->scene.spec_prefiltered_tex) {
		glDeleteTextures(1, &app->scene.spec_prefiltered_tex);
	}
	if (app->scene.irradiance_tex) {
		glDeleteTextures(1, &app->scene.irradiance_tex);
	}

	app->scene.hdr_texture = hdr_tex;
	app->scene.spec_prefiltered_tex = spec_tex;
	app->scene.irradiance_tex = irr_tex;

	LOG_INFO("suckless-ogl.app", "[Frame %llu] Environment swap finalized.",
	         (unsigned long long)app->frame_count);
}

static void handle_ibl_done_wait_state(App* app)
{
	GLuint hdr_tex = 0;
	GLuint spec_tex = 0;
	GLuint irr_tex = 0;
	float threshold = 0.0F;

	if (ibl_coordinator_get_results(&app->scene.ibl_coord, &hdr_tex,
	                                &spec_tex, &irr_tex, &threshold)) {
		finalize_ibl_swap(app, hdr_tex, spec_tex, irr_tex, threshold);
		app->env_mgr.transition_state = TRANSITION_FADE_IN;
		app->env_mgr.transition_alpha = 1.0F;
	}
}

static void handle_ibl_done_loading_state(App* app)
{
	if (app->env_mgr.env_transition_mode == ENV_TRANSITION_BLACK_SCREEN) {
		/* Black Screen mode: Start fading out to black.
		 * Do NOT consume results yet (wait for fade out). */
		app->env_mgr.transition_state = TRANSITION_FADE_OUT;
		app->env_mgr.transition_alpha = 0.0F;
	} else {
		/* Crossfade mode: Capture, swap and fade in */
		GLuint hdr_tex = 0;
		GLuint spec_tex = 0;
		GLuint irr_tex = 0;
		float threshold = 0.0F;

		if (ibl_coordinator_get_results(&app->scene.ibl_coord, &hdr_tex,
		                                &spec_tex, &irr_tex,
		                                &threshold)) {
			capture_snapshot(app);
			finalize_ibl_swap(app, hdr_tex, spec_tex, irr_tex,
			                  threshold);
			app->env_mgr.transition_state = TRANSITION_FADE_IN;
			app->env_mgr.transition_alpha = 1.0F;
		}
	}
}

void app_process_ibl_state_machine(App* app)
{
	IBLState state =
	    ibl_coordinator_update(&app->scene.ibl_coord, app->frame_count);

	if (state == IBL_STATE_DONE) {
		if (app->env_mgr.transition_state == TRANSITION_WAIT_IBL) {
			handle_ibl_done_wait_state(app);
		} else if (app->env_mgr.transition_state ==
		           TRANSITION_LOADING) {
			handle_ibl_done_loading_state(app);
		}
	}
}

void app_update_transition(App* app)
{
	switch (app->env_mgr.transition_state) {
		case TRANSITION_IDLE:
		case TRANSITION_LOADING:
		case TRANSITION_WAIT_IBL:
			break;

		case TRANSITION_FADE_OUT:
			app->env_mgr.transition_alpha +=
			    (float)app->delta_time /
			    app->env_mgr.transition_duration;
			if (app->env_mgr.transition_alpha >= 1.0F) {
				app->env_mgr.transition_alpha = 1.0F;

				/* BLACK SCREEN SWAP HAPPENS HERE */
				GLuint hdr_tex = 0;
				GLuint spec_tex = 0;
				GLuint irr_tex = 0;
				float threshold = 0.0F;
				if (ibl_coordinator_get_results(
				        &app->scene.ibl_coord, &hdr_tex,
				        &spec_tex, &irr_tex, &threshold)) {
					finalize_ibl_swap(app, hdr_tex,
					                  spec_tex, irr_tex,
					                  threshold);
					app->env_mgr.transition_state =
					    TRANSITION_FADE_IN;
				}
			}
			break;

		case TRANSITION_FADE_IN:
			app->env_mgr.transition_alpha -=
			    (float)app->delta_time /
			    app->env_mgr.transition_duration;
			if (app->env_mgr.transition_alpha <= 0.0F) {
				app->env_mgr.transition_alpha = 0.0F;
				app->env_mgr.transition_state = TRANSITION_IDLE;
			}
			break;
	}
}
