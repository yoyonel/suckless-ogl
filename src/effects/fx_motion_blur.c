#include "fx_motion_blur.h"

#include "gl_common.h"
#include "log.h"
#include "postprocess.h"
#include "shader.h"
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <stddef.h>

/* Compute Shader Constants */
enum { MB_COMPUTE_GROUP_SIZE = 16 };

/* Motion Blur Constants (Default values) */
static const float DEFAULT_MB_INTENSITY = 1.0F;
static const float DEFAULT_MB_MAX_VELOCITY = 0.05F;
static const int DEFAULT_MB_SAMPLES = 8;

int fx_motion_blur_init(PostProcess* post_processing)
{
	MotionBlurFX* mb_fx = &post_processing->motion_blur_fx;

	/* 1. Initialiser les paramètres par défaut dans PostProcess */
	post_processing->motion_blur.intensity = DEFAULT_MB_INTENSITY;
	post_processing->motion_blur.max_velocity = DEFAULT_MB_MAX_VELOCITY;
	post_processing->motion_blur.samples = DEFAULT_MB_SAMPLES;

	/* 2. Charger les shaders */
	mb_fx->tile_max_shader =
	    shader_load_compute_program("shaders/tile_max_velocity.comp");
	mb_fx->neighbor_max_shader =
	    shader_load_compute_program("shaders/neighbor_max_velocity.comp");

	if (!mb_fx->tile_max_shader || !mb_fx->neighbor_max_shader) {
		LOG_ERROR("suckless-ogl.effects.motion_blur",
		          "Failed to load motion blur compute shaders");
		return 0;
	}

	/* 3. Initialiser les matrices */
	glm_mat4_identity(mb_fx->previous_view_proj);

	/* 4. Créer les textures */
	return fx_motion_blur_resize(post_processing);
}

void fx_motion_blur_cleanup(PostProcess* post_processing)
{
	MotionBlurFX* mb_fx = &post_processing->motion_blur_fx;

	if (mb_fx->tile_max_tex) {
		glDeleteTextures(1, &mb_fx->tile_max_tex);
		mb_fx->tile_max_tex = 0;
	}
	if (mb_fx->neighbor_max_tex) {
		glDeleteTextures(1, &mb_fx->neighbor_max_tex);
		mb_fx->neighbor_max_tex = 0;
	}
	if (mb_fx->tile_max_shader) {
		shader_destroy(mb_fx->tile_max_shader);
		mb_fx->tile_max_shader = NULL;
	}
	if (mb_fx->neighbor_max_shader) {
		shader_destroy(mb_fx->neighbor_max_shader);
		mb_fx->neighbor_max_shader = NULL;
	}
}

int fx_motion_blur_resize(PostProcess* post_processing)
{
	MotionBlurFX* mb_fx = &post_processing->motion_blur_fx;

	int tile_width =
	    (post_processing->width + (MB_COMPUTE_GROUP_SIZE - 1)) /
	    MB_COMPUTE_GROUP_SIZE;
	int tile_height =
	    (post_processing->height + (MB_COMPUTE_GROUP_SIZE - 1)) /
	    MB_COMPUTE_GROUP_SIZE;

	if (tile_width < 1) {
		tile_width = 1;
	}
	if (tile_height < 1) {
		tile_height = 1;
	}

	/* Tile Max Texture (RG16F) */
	if (mb_fx->tile_max_tex) {
		glDeleteTextures(1, &mb_fx->tile_max_tex);
	}
	glGenTextures(1, &mb_fx->tile_max_tex);
	glBindTexture(GL_TEXTURE_2D, mb_fx->tile_max_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, tile_width, tile_height, 0,
	             GL_RG, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	/* Neighbor Max Texture (RG16F) */
	if (mb_fx->neighbor_max_tex) {
		glDeleteTextures(1, &mb_fx->neighbor_max_tex);
	}
	glGenTextures(1, &mb_fx->neighbor_max_tex);
	glBindTexture(GL_TEXTURE_2D, mb_fx->neighbor_max_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, tile_width, tile_height, 0,
	             GL_RG, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
	return 1;
}

void fx_motion_blur_render(PostProcess* post_processing)
{
	MotionBlurFX* mb_fx = &post_processing->motion_blur_fx;

	int groups_x = (post_processing->width + (MB_COMPUTE_GROUP_SIZE - 1)) /
	               MB_COMPUTE_GROUP_SIZE;
	int groups_y = (post_processing->height + (MB_COMPUTE_GROUP_SIZE - 1)) /
	               MB_COMPUTE_GROUP_SIZE;

	/* Pass 1: Tile Max Velocity */
	shader_use(mb_fx->tile_max_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->velocity_tex);
	shader_set_int(mb_fx->tile_max_shader, "velocityTexture", 0);

	glBindImageTexture(1, mb_fx->tile_max_tex, 0, GL_FALSE, 0,
	                   GL_WRITE_ONLY, GL_RG16F);

	glDispatchCompute((GLuint)groups_x, (GLuint)groups_y, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
	                GL_TEXTURE_FETCH_BARRIER_BIT);

	/* Pass 2: Neighbor Max Velocity */
	shader_use(mb_fx->neighbor_max_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mb_fx->tile_max_tex);
	shader_set_int(mb_fx->neighbor_max_shader, "tileMaxTexture", 0);

	glBindImageTexture(1, mb_fx->neighbor_max_tex, 0, GL_FALSE, 0,
	                   GL_WRITE_ONLY, GL_RG16F);

	glDispatchCompute((GLuint)groups_x, (GLuint)groups_y, 1);
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}

void fx_motion_blur_update_matrices(PostProcess* post_processing,
                                    mat4 view_proj)
{
	MotionBlurFX* mb_fx = &post_processing->motion_blur_fx;
	glm_mat4_copy(view_proj, mb_fx->previous_view_proj);
}

void fx_motion_blur_upload_params(Shader* shader,
                                  const MotionBlurParams* params)
{
	shader_set_float(shader, "motionBlur.intensity", params->intensity);
	shader_set_float(shader, "motionBlur.maxVelocity",
	                 params->max_velocity);
	shader_set_int(shader, "motionBlur.samples", params->samples);
}
