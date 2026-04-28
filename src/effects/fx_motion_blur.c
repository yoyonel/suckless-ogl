#include "effects/fx_motion_blur.h"

#include "effects/effect_context.h"
#include "effects/fx_utils.h"
#include "gl_common.h"
#include "log.h"
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

int fx_motion_blur_init(MotionBlurFX* mb_fx, MotionBlurParams* params)
{
	/* 1. Initialiser les paramètres par défaut */
	params->intensity = DEFAULT_MB_INTENSITY;
	params->max_velocity = DEFAULT_MB_MAX_VELOCITY;
	params->samples = DEFAULT_MB_SAMPLES;

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

	/* Set sampler uniforms once (they are per-program state) */
	shader_use(mb_fx->tile_max_shader);
	shader_set_int(mb_fx->tile_max_shader, "velocityTexture", 0);
	shader_use(mb_fx->neighbor_max_shader);
	shader_set_int(mb_fx->neighbor_max_shader, "tileMaxTexture", 0);

	/* 3. Initialiser les matrices */
	glm_mat4_identity(mb_fx->previous_view_proj);

	return 1;
}

void fx_motion_blur_cleanup(MotionBlurFX* mb_fx)
{
	if (mb_fx->tile_max_tex) {
		glDeleteTextures(1, &mb_fx->tile_max_tex);
		mb_fx->tile_max_tex = 0;
	}
	if (mb_fx->neighbor_max_tex) {
		glDeleteTextures(1, &mb_fx->neighbor_max_tex);
		mb_fx->neighbor_max_tex = 0;
	}
	SHADER_SAFE_DESTROY(mb_fx->tile_max_shader);
	SHADER_SAFE_DESTROY(mb_fx->neighbor_max_shader);
}

int fx_motion_blur_resize(MotionBlurFX* mb_fx, int width, int height)
{
	/* Ensure Unit 0 is active for initial texture setup */
	glActiveTexture(GL_TEXTURE0);

	int tile_width =
	    (width + (MB_COMPUTE_GROUP_SIZE - 1)) / MB_COMPUTE_GROUP_SIZE;
	int tile_height =
	    (height + (MB_COMPUTE_GROUP_SIZE - 1)) / MB_COMPUTE_GROUP_SIZE;

	if (tile_width < 1) {
		tile_width = 1;
	}
	if (tile_height < 1) {
		tile_height = 1;
	}

	FXTextureConfig mb_config = {.width = tile_width,
	                             .height = tile_height,
	                             .internal_format = GL_RG16F,
	                             .format = GL_RG,
	                             .type = GL_FLOAT,
	                             .min_filter = GL_NEAREST,
	                             .mag_filter = GL_NEAREST,
	                             .wrap_s = GL_CLAMP_TO_EDGE,
	                             .wrap_t = GL_CLAMP_TO_EDGE,
	                             .initial_data = NULL};

	/* Tile Max Texture (RG16F) */
	fx_utils_create_texture(&mb_fx->tile_max_tex, &mb_config);

	/* Neighbor Max Texture (RG16F) */
	fx_utils_create_texture(&mb_fx->neighbor_max_tex, &mb_config);

	return 1;
}

void fx_motion_blur_render(MotionBlurFX* mb_fx, const EffectContext* ctx)
{
	/* Tile dimensions (one tile per 16x16 pixel block) */
	int tile_count_x =
	    (ctx->width + (MB_COMPUTE_GROUP_SIZE - 1)) / MB_COMPUTE_GROUP_SIZE;
	int tile_count_y =
	    (ctx->height + (MB_COMPUTE_GROUP_SIZE - 1)) / MB_COMPUTE_GROUP_SIZE;

	/* Pass 1: Tile Max Velocity
	 * Each workgroup (16x16 threads) processes one 16x16 pixel tile,
	 * reducing to a single max velocity per tile via shared memory.
	 * Dispatch: one group per tile. */
	shader_use(mb_fx->tile_max_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ctx->velocity_tex);

	glBindImageTexture(1, mb_fx->tile_max_tex, 0, GL_FALSE, 0,
	                   GL_WRITE_ONLY, GL_RG16F);

	glDispatchCompute((GLuint)tile_count_x, (GLuint)tile_count_y, 1);
	glMemoryBarrier((GLbitfield)GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
	                (GLbitfield)GL_TEXTURE_FETCH_BARRIER_BIT);

	/* Pass 2: Neighbor Max Velocity
	 * Each thread processes one tile (not one pixel).
	 * Input is tile_max_tex of size tile_count_x × tile_count_y.
	 * Each workgroup (16x16 threads) covers 16x16 tiles.
	 * Dispatch: ceil(tile_count / 16) groups. */
	shader_use(mb_fx->neighbor_max_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mb_fx->tile_max_tex);

	glBindImageTexture(1, mb_fx->neighbor_max_tex, 0, GL_FALSE, 0,
	                   GL_WRITE_ONLY, GL_RG16F);

	int neighbor_groups_x = (tile_count_x + (MB_COMPUTE_GROUP_SIZE - 1)) /
	                        MB_COMPUTE_GROUP_SIZE;
	int neighbor_groups_y = (tile_count_y + (MB_COMPUTE_GROUP_SIZE - 1)) /
	                        MB_COMPUTE_GROUP_SIZE;

	glDispatchCompute((GLuint)neighbor_groups_x, (GLuint)neighbor_groups_y,
	                  1);
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

	/* NOTE: On Mesa/Intel Iris Xe, GPU profiler queries
	 * (GL_TIMESTAMP and GL_TIME_ELAPSED) report near-zero timings
	 * for compute dispatches.  This is a known driver limitation —
	 * the compute work executes correctly but the queries complete
	 * before the dispatch finishes.  The profiled duration shown
	 * for this stage is therefore unreliable on this driver. */
}

void fx_motion_blur_update_matrices(MotionBlurFX* mb_fx, mat4 view_proj)
{
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
