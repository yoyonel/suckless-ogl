#include "env_manager.h"

#include "app_settings.h"
#include "async_loader.h"
#include "gl_common.h"
#include "ibl_coordinator.h"
#include "log.h"
#include "postprocess.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "utils.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

static const int MAX_PATH_LENGTH = 256;

int env_manager_load(EnvManager* mgr, AsyncLoader* loader, const char* filename)
{
	if (!is_safe_filename(filename)) {
		LOG_ERROR("suckless-ogl.env",
		          "Security Violation: Invalid filename rejected: %s",
		          filename ? filename : "NULL");
		return 0;
	}

	char path[MAX_PATH_LENGTH];
	if (safe_snprintf(path, sizeof(path), "assets/textures/hdr/%s",
	                  filename) < 0) {
		LOG_ERROR("suckless-ogl.env", "Filename too long: %s",
		          filename);
		return false;
	}

	LOG_INFO("suckless-ogl.env", "Queuing async load for: %s", path);
	if (async_loader_request(loader, path)) {
		mgr->env_map_loading = 1;
		return 1;
	}
	LOG_WARNING("suckless-ogl.env",
	            "Async load request failed/ignored for: %s", path);
	return 0;
}

void env_manager_process_loading_step(EnvManager* mgr, GLuint* recycled_hdr_tex,
                                      IBLCoordinator* ibl)
{
	if (mgr->env_map_loading_step == 0) {
		return;
	}

	AsyncRequest* req = &mgr->current_env_req;

	if (mgr->env_map_loading_step == 1) {
		/* Step 1: Upload texture to GPU (No Mipmaps) */
		if (!req->half_data && !req->pbo_mapped_ptr) {
			LOG_ERROR("suckless-ogl.env",
			          "Async request data is NULL!");
			mgr->env_map_loading = 0;
			mgr->env_map_loading_step = 0;
			return;
		}

		LOG_INFO("suckless-ogl.env",
		         "Finalizing environment load (Step 1/3): Upload...");
		mgr->pending_env_tex = texture_upload_hdr_from_pbo(
		    req->pbo_id, req->pbo_mapped_ptr, req->width, req->height,
		    *recycled_hdr_tex);

		if (mgr->pending_env_tex != 0) {
			*recycled_hdr_tex = 0;
		}

		/* If this was a CPU fallback (malloc), free the buffer now */
		if (req->pbo_id == 0 && req->pbo_mapped_ptr != NULL) {
			free(req->pbo_mapped_ptr);
			req->pbo_mapped_ptr = NULL;
		}

		if (req->half_data) {
			free(req->half_data);
			req->half_data = NULL;
		}

		mgr->env_map_loading_step = 2; /* Next frame */

	} else if (mgr->env_map_loading_step == 2) {
		/* Step 2: Generate Mipmaps */
		LOG_INFO("suckless-ogl.env",
		         "Finalizing environment load (Step 2/3): Mipmaps...");
		if (mgr->pending_env_tex) {
			texture_generate_hdr_mipmap(mgr->pending_env_tex);
			mgr->env_map_loading_step = 3; /* Next frame */
		} else {
			LOG_ERROR("suckless-ogl.env",
			          "Failed to create texture from HDR data!");
			mgr->env_map_loading = 0;
			mgr->env_map_loading_step = 0;
		}

	} else if (mgr->env_map_loading_step == 3) {
		/* Step 3: Start IBL Coordinator */
		LOG_INFO(
		    "suckless-ogl.env",
		    "Finalizing environment load (Step 3/3): Start IBL...");
		if (mgr->pending_env_tex) {
			ibl_coordinator_start(ibl, mgr->pending_env_tex,
			                      req->width, req->height);
			mgr->pending_env_tex = 0;
		}
		mgr->env_map_loading = 0;
		mgr->env_map_loading_step = 0;
	}
}

int env_manager_trigger_transition(EnvManager* mgr, AsyncLoader* loader,
                                   const char* filename)
{
	/* Don't trigger if already fading or loading */
	if (mgr->transition_state != TRANSITION_IDLE) {
		return 0;
	}

	/* Start loading in background */
	mgr->transition_state = TRANSITION_LOADING;
	mgr->transition_alpha = 0.0F;

	return env_manager_load(mgr, loader, filename);
}

static void capture_snapshot(Scene* scene, int width, int height)
{
	if (scene->transition_snapshot_tex == 0) {
		glGenTextures(1, &scene->transition_snapshot_tex);
	}
	glBindTexture(GL_TEXTURE_2D, scene->transition_snapshot_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
	             GL_UNSIGNED_BYTE, NULL);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void finalize_ibl_swap(Scene* scene, PostProcess* postproc,
                              GLuint hdr_tex, GLuint spec_tex, GLuint irr_tex,
                              float threshold, uint64_t frame_count)
{
	postprocess_set_exposure_target(postproc, threshold);

	/* Recycle the old HDR texture instead of deleting it */
	if (scene->hdr_texture) {
		if (scene->recycled_hdr_tex) {
			/* If we already have one (weird edge case),
			 * delete the old one */
			glDeleteTextures(1, &scene->recycled_hdr_tex);
		}
		scene->recycled_hdr_tex = scene->hdr_texture;
		/* scene->hdr_texture will be overwritten below */
	}

	if (scene->spec_prefiltered_tex) {
		glDeleteTextures(1, &scene->spec_prefiltered_tex);
	}
	if (scene->irradiance_tex) {
		glDeleteTextures(1, &scene->irradiance_tex);
	}

	scene->hdr_texture = hdr_tex;
	scene->spec_prefiltered_tex = spec_tex;
	scene->irradiance_tex = irr_tex;

	LOG_INFO("suckless-ogl.env", "[Frame %llu] Environment swap finalized.",
	         (unsigned long long)frame_count);
}

static void handle_ibl_done_wait_state(EnvManager* mgr, Scene* scene,
                                       PostProcess* postproc,
                                       uint64_t frame_count)
{
	GLuint hdr_tex = 0;
	GLuint spec_tex = 0;
	GLuint irr_tex = 0;
	float threshold = 0.0F;

	if (ibl_coordinator_get_results(&scene->ibl_coord, &hdr_tex, &spec_tex,
	                                &irr_tex, &threshold)) {
		finalize_ibl_swap(scene, postproc, hdr_tex, spec_tex, irr_tex,
		                  threshold, frame_count);
		mgr->transition_state = TRANSITION_FADE_IN;
		mgr->transition_alpha = 1.0F;
	}
}

static void handle_ibl_done_loading_state(EnvManager* mgr, Scene* scene,
                                          PostProcess* postproc,
                                          uint64_t frame_count, int width,
                                          int height)
{
	if (mgr->env_transition_mode == ENV_TRANSITION_BLACK_SCREEN) {
		/* Black Screen mode: Start fading out to black.
		 * Do NOT consume results yet (wait for fade out). */
		mgr->transition_state = TRANSITION_FADE_OUT;
		mgr->transition_alpha = 0.0F;
	} else {
		/* Crossfade mode: Capture, swap and fade in */
		GLuint hdr_tex = 0;
		GLuint spec_tex = 0;
		GLuint irr_tex = 0;
		float threshold = 0.0F;

		if (ibl_coordinator_get_results(&scene->ibl_coord, &hdr_tex,
		                                &spec_tex, &irr_tex,
		                                &threshold)) {
			capture_snapshot(scene, width, height);
			finalize_ibl_swap(scene, postproc, hdr_tex, spec_tex,
			                  irr_tex, threshold, frame_count);
			mgr->transition_state = TRANSITION_FADE_IN;
			mgr->transition_alpha = 1.0F;
		}
	}
}

void env_manager_update_ibl(EnvManager* mgr, Scene* scene,
                            PostProcess* postproc, uint64_t frame_count,
                            int width, int height)
{
	IBLState state = ibl_coordinator_update(&scene->ibl_coord, frame_count);

	if (state == IBL_STATE_DONE) {
		if (mgr->transition_state == TRANSITION_WAIT_IBL) {
			handle_ibl_done_wait_state(mgr, scene, postproc,
			                           frame_count);
		} else if (mgr->transition_state == TRANSITION_LOADING) {
			handle_ibl_done_loading_state(
			    mgr, scene, postproc, frame_count, width, height);
		}
	}
}

void env_manager_update_transition(EnvManager* mgr, Scene* scene,
                                   PostProcess* postproc, double delta_time,
                                   uint64_t frame_count)
{
	switch (mgr->transition_state) {
		case TRANSITION_IDLE:
		case TRANSITION_LOADING:
		case TRANSITION_WAIT_IBL:
			break;

		case TRANSITION_FADE_OUT:
			mgr->transition_alpha +=
			    (float)delta_time / mgr->transition_duration;
			if (mgr->transition_alpha < 1.0F) {
				break;
			}
			mgr->transition_alpha = 1.0F;

			/* BLACK SCREEN SWAP HAPPENS HERE */
			GLuint hdr_tex = 0;
			GLuint spec_tex = 0;
			GLuint irr_tex = 0;
			float threshold = 0.0F;
			if (ibl_coordinator_get_results(&scene->ibl_coord,
			                                &hdr_tex, &spec_tex,
			                                &irr_tex, &threshold)) {
				finalize_ibl_swap(scene, postproc, hdr_tex,
				                  spec_tex, irr_tex, threshold,
				                  frame_count);
				mgr->transition_state = TRANSITION_FADE_IN;
			}
			break;

		case TRANSITION_FADE_IN:
			mgr->transition_alpha -=
			    (float)delta_time / mgr->transition_duration;
			if (mgr->transition_alpha <= 0.0F) {
				mgr->transition_alpha = 0.0F;
				mgr->transition_state = TRANSITION_IDLE;
			}
			break;
	}
}

void env_manager_render_overlay(const EnvManager* mgr, const Scene* scene)
{
	if (mgr->transition_state != TRANSITION_IDLE) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_DEPTH_TEST);

		shader_use(scene->debug_shader);
		shader_set_int(scene->debug_shader, "u_tex", 0);
		shader_set_float(scene->debug_shader, "u_alpha",
		                 mgr->transition_alpha);
		shader_set_int(scene->debug_shader, "u_bypass_processing", 1);
		shader_set_float(scene->debug_shader, "lod", 0.0F);

		glActiveTexture(GL_TEXTURE0);
		if (mgr->env_transition_mode == ENV_TRANSITION_CROSSFADE &&
		    scene->transition_snapshot_tex != 0 &&
		    mgr->transition_state == TRANSITION_FADE_IN) {
			/* Crossfade: Bind snapshot texture */
			glBindTexture(GL_TEXTURE_2D,
			              scene->transition_snapshot_tex);
		} else {
			/* Black Screen / Initial Load: Bind dummy black */
			glBindTexture(GL_TEXTURE_2D, scene->dummy_black_tex);
		}

		glBindVertexArray(scene->quad_vbo);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);

		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
	}
}
