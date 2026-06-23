#include "env_manager.h"

#include "app.h"
#include "app_settings.h"
#include "asset_manager.h"
#include "async_loader.h"
#include "gl_common.h"
#include "ibl_coordinator.h"
#include "log.h"
#include "platform/platform_utils.h"
#include "postprocess_readback.h"
#include "scene.h"
#include "scene_gpu_resources.h"
#include "scene_shaders.h"
#include "shader.h"
#include "texture.h"
#include "utils.h"
#include <stdint.h>
#include <stdlib.h>

static const int MAX_PATH_LENGTH = 256;

int env_manager_load(EnvManager* mgr, AsyncLoader* loader, const char* filename)
{
	if (!is_safe_filename(filename)) {
		LOG_ERROR("suckless-ogl.env",
		          "Security Violation: Invalid filename rejected: %s",
		          filename ? filename : "NULL");
		return 0;
	}

	AssetHandle handle;
	/* On demande à l'Asset Manager de résoudre le chemin complet et le type
	 */
	if (!asset_resolve_path(ASSET_DIR_HDR, filename, &handle)) {
		LOG_ERROR(
		    "suckless-ogl.env",
		    "Unsupported asset format or path resolution failed: %s",
		    filename);
		return 0;
	}

	LOG_INFO("suckless-ogl.env", "Queuing async load for: %s (Type: %d)",
	         handle.full_path, handle.type);

	/* On passe le handle typé plutôt que le chemin */
	if (async_loader_request(loader, &handle)) {
		mgr->env_map_loading = true;
		return 1;
	}

	LOG_WARNING("suckless-ogl.env",
	            "Async load request failed/ignored for: %s",
	            handle.full_path);
	return 0;
}

void env_manager_process_loading_step(EnvManager* mgr, GLuint* recycled_hdr_tex,
                                      IBLCoordinator* ibl)
{
	if (mgr->env_map_loading_step == EML_STOP) {
		return;
	}

	AsyncRequest* req = &mgr->current_env_req;

	if (mgr->env_map_loading_step == EML_UPLOAD_HDR_FROM_PBO) {
		/* Étape 1 : Transfert de la texture vers le GPU (Sans Mipmaps)
		 * Grâce aux backends d'implémentation, req->pbo_mapped_ptr
		 * contient toujours les données décodées finales (qu'il
		 * s'agisse d'un vrai PBO ou d'un fallback CPU). */
		if (!req->pbo_mapped_ptr) {
			LOG_ERROR("suckless-ogl.env",
			          "Async request data is NULL!");
			mgr->env_map_loading = false;
			mgr->env_map_loading_step = EML_STOP;
			return;
		}

		LOG_INFO("suckless-ogl.env",
		         "Finalizing environment load (Step 1/3): Upload...");
		mgr->pending_env_tex = texture_upload_hdr_from_pbo(
		    req->pbo_id, req->pbo_mapped_ptr, req->width, req->height,
		    *recycled_hdr_tex,
		    req->gl_internal_format,  // ex: GL_RGBA16F ou GL_RGB16F
		    req->gl_format,           // ex: GL_RGBA ou GL_RGB
		    req->gl_type,             // ex: GL_HALF_FLOAT
		    req->is_compressed,       // ex: false
		    (GLsizei)req->required_pbo_size);

		if (mgr->pending_env_tex != 0) {
			*recycled_hdr_tex = 0;
		}

		/* Si pbo_id == 0, cela signifie qu'un fallback malloc CPU a été
		 * utilisé. On libère la mémoire ici sur le thread principal
		 * après l'upload. */
		if (req->pbo_id == 0 && req->pbo_mapped_ptr != NULL) {
			free(req->pbo_mapped_ptr);
			req->pbo_mapped_ptr = NULL;
		}

		mgr->env_map_loading_step = EML_MIPMAP; /* Prochaine frame */

	} else if (mgr->env_map_loading_step == EML_MIPMAP) {
		/* Étape 2 : Génération de la chaîne de Mipmaps */
		LOG_INFO("suckless-ogl.env",
		         "Finalizing environment load (Step 2/3): Mipmaps...");
		if (mgr->pending_env_tex) {
			texture_generate_hdr_mipmap(mgr->pending_env_tex);
			mgr->env_map_loading_step =
			    EML_START_IBL; /* Prochaine frame */
		} else {
			LOG_ERROR("suckless-ogl.env",
			          "Failed to create texture from HDR data!");
			mgr->env_map_loading = false;
			mgr->env_map_loading_step = 0;
		}

	} else if (mgr->env_map_loading_step == EML_START_IBL) {
		/* Étape 3 : Lancement du traitement IBL synchrone étalé */
		LOG_INFO(
		    "suckless-ogl.env",
		    "Finalizing environment load (Step 3/3): Start IBL...");
		if (mgr->pending_env_tex) {
			ibl_coordinator_start(ibl, mgr->pending_env_tex,
			                      req->width, req->height);
			mgr->pending_env_tex = 0;
		}
		mgr->env_map_loading = false;
		mgr->env_map_loading_step = EML_STOP;
	}
}

int env_manager_trigger_transition(EnvManager* mgr, AsyncLoader* loader,
                                   const char* filename)
{
	/* Évite les doubles transitions si déjà en chargement */
	if (mgr->transition_state != TRANSITION_IDLE) {
		return 0;
	}

	mgr->transition_state = TRANSITION_LOADING;
	mgr->transition_alpha = 0.0F;

	return env_manager_load(mgr, loader, filename);
}

static void capture_snapshot(Scene* scene, int width, int height)
{
	if (scene->gpu->transition_snapshot_tex == 0) {
		glGenTextures(1, &scene->gpu->transition_snapshot_tex);
	}
	glBindTexture(GL_TEXTURE_2D, scene->gpu->transition_snapshot_tex);
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

	/* Recyclage de l'ancienne texture HDR pour éviter les allocations
	 * coûteuses */
	if (scene->gpu->hdr_texture) {
		if (scene->gpu->recycled_hdr_tex) {
			glDeleteTextures(1, &scene->gpu->recycled_hdr_tex);
		}
		scene->gpu->recycled_hdr_tex = scene->gpu->hdr_texture;
	}

	if (scene->gpu->spec_prefiltered_tex) {
		glDeleteTextures(1, &scene->gpu->spec_prefiltered_tex);
	}
	if (scene->gpu->irradiance_tex) {
		glDeleteTextures(1, &scene->gpu->irradiance_tex);
	}

	scene->gpu->hdr_texture = hdr_tex;
	scene->gpu->spec_prefiltered_tex = spec_tex;
	scene->gpu->irradiance_tex = irr_tex;

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

	if (ibl_coordinator_get_results(&scene->lighting.ibl_coord, &hdr_tex,
	                                &spec_tex, &irr_tex, &threshold)) {
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
		mgr->transition_state = TRANSITION_FADE_OUT;
		mgr->transition_alpha = 0.0F;
	} else {
		GLuint hdr_tex = 0;
		GLuint spec_tex = 0;
		GLuint irr_tex = 0;
		float threshold = 0.0F;

		if (ibl_coordinator_get_results(&scene->lighting.ibl_coord,
		                                &hdr_tex, &spec_tex, &irr_tex,
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
	IBLState state =
	    ibl_coordinator_update(&scene->lighting.ibl_coord, frame_count);

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

			GLuint hdr_tex = 0;
			GLuint spec_tex = 0;
			GLuint irr_tex = 0;
			float threshold = 0.0F;
			if (ibl_coordinator_get_results(
			        &scene->lighting.ibl_coord, &hdr_tex, &spec_tex,
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

		shader_use(scene->shaders->debug);
		shader_set_int(scene->shaders->debug, "u_tex", 0);
		shader_set_float(scene->shaders->debug, "u_alpha",
		                 mgr->transition_alpha);
		shader_set_int(scene->shaders->debug, "u_bypass_processing", 1);
		shader_set_float(scene->shaders->debug, "lod", 0.0F);

		glActiveTexture(GL_TEXTURE0);
		if (mgr->env_transition_mode == ENV_TRANSITION_CROSSFADE &&
		    scene->gpu->transition_snapshot_tex != 0 &&
		    mgr->transition_state == TRANSITION_FADE_IN) {
			glBindTexture(GL_TEXTURE_2D,
			              scene->gpu->transition_snapshot_tex);
		} else {
			glBindTexture(GL_TEXTURE_2D,
			              scene->gpu->dummy_black_tex);
		}

		glBindVertexArray(scene->gpu->quad_vbo);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);

		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
	}
}

int env_mgr_subsys_init(App* app)
{
	app->env_mgr =
	    platform_aligned_alloc(sizeof(*app->env_mgr), SIMD_ALIGNMENT);
	if (!app->env_mgr) {
		return 0;
	}
	*app->env_mgr = (EnvManager){.env_map_loading = false};
	app->env_mgr->is_first_load = true;
	app->env_mgr->transition_state = TRANSITION_WAIT_IBL;
	app->env_mgr->transition_alpha = 1.0F;
	app->env_mgr->transition_duration = DEFAULT_ENV_TRANSITION_DURATION;
	app->env_mgr->env_transition_mode = DEFAULT_ENV_TRANSITION_MODE;
	return 1;
}

void env_mgr_subsys_cleanup(App* app)
{
	if (app->env_mgr) {
		platform_aligned_free(app->env_mgr);
		app->env_mgr = NULL;
	}
}
