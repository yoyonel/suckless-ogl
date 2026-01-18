#include "postprocess.h"

#include "gl_common.h"
#include "log.h"
#include "shader.h"
#include <string.h>

static int create_framebuffer(PostProcess* post_processing);
static int create_bloom_resources(PostProcess* post_processing);
static int create_screen_quad(PostProcess* post_processing);
static void destroy_framebuffer(PostProcess* post_processing);
static void destroy_bloom_resources(PostProcess* post_processing);
static void destroy_screen_quad(PostProcess* post_processing);
static void render_bloom(PostProcess* post_processing);

/* TODO: move to gl_common.h and mutualize with skybox rendering */
enum { SCREEN_QUAD_VERTEX_COUNT = 6 };

/* Vertices pour un quad plein écran */
static const float screen_quad_vertices[SCREEN_QUAD_VERTEX_COUNT * (2 + 2)] =
    {/* positions     texCoords */
     -1.0F, 1.0F, 0.0F, 1.0F,  -1.0F, -1.0F,
     0.0F,  0.0F, 1.0F, -1.0F, 1.0F,  0.0F,

     -1.0F, 1.0F, 0.0F, 1.0F,  1.0F,  -1.0F,
     1.0F,  0.0F, 1.0F, 1.0F,  1.0F,  1.0F};

int postprocess_init(PostProcess* post_processing, int width, int height)
{
	*post_processing = (PostProcess){0};

	post_processing->width = width;
	post_processing->height = height;
	post_processing->time = 0.0F;

	/* Paramètres par défaut */
	post_processing->vignette.intensity = DEFAULT_VIGNETTE_INTENSITY;
	post_processing->vignette.extent = DEFAULT_VIGNETTE_EXTENT;
	post_processing->grain.intensity = DEFAULT_GRAIN_INTENSITY;
	post_processing->exposure.exposure = DEFAULT_EXPOSURE;
	post_processing->chrom_abbr.strength = DEFAULT_CHROM_ABBR_STRENGTH;

	/* Bloom defaults (Off) */
	post_processing->bloom.intensity = DEFAULT_BLOOM_INTENSITY;
	post_processing->bloom.threshold = DEFAULT_BLOOM_THRESHOLD;
	post_processing->bloom.soft_threshold = DEFAULT_BLOOM_SOFT_THRESHOLD;
	post_processing->bloom.radius = DEFAULT_BLOOM_RADIUS;

	/* Initialisation Color Grading (Neutre) */
	post_processing->color_grading.saturation = 1.0F;
	post_processing->color_grading.contrast = 1.0F;
	post_processing->color_grading.gamma = 1.0F;
	post_processing->color_grading.gain = 1.0F;
	post_processing->color_grading.offset = 0.0F;

	/* Effets désactivés par défaut */
	post_processing->active_effects = 0;

	/* Créer le framebuffer */
	if (!create_framebuffer(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create framebuffer");
		return 0;
	}

	/* Créer les ressources Bloom */
	if (!create_bloom_resources(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create bloom resources");
		destroy_framebuffer(post_processing);
		return 0;
	}

	/* Créer le quad plein écran */
	if (!create_screen_quad(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create screen quad");
		destroy_framebuffer(post_processing);
		destroy_bloom_resources(post_processing);
		return 0;
	}

	/* Charger le shader de post-processing */
	post_processing->postprocess_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/postprocess.frag");

	if (!post_processing->postprocess_shader) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to load postprocess shader");
		destroy_framebuffer(post_processing);
		destroy_bloom_resources(post_processing);
		destroy_screen_quad(post_processing);
		return 0;
	}

	post_processing->bloom_prefilter_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/bloom_prefilter.frag");
	post_processing->bloom_downsample_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/bloom_downsample.frag");
	post_processing->bloom_upsample_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/bloom_upsample.frag");

	if (!post_processing->bloom_prefilter_shader ||
	    !post_processing->bloom_downsample_shader ||
	    !post_processing->bloom_upsample_shader) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to load bloom shaders");
		/* On continue quand même, le bloom ne marchera juste pas */
	}

	LOG_INFO("suckless-ogl.postprocess",
	         "Post-processing initialized (%dx%d)", width, height);

	return 1;
}

void postprocess_cleanup(PostProcess* post_processing)
{
	destroy_framebuffer(post_processing);
	destroy_screen_quad(post_processing);

	if (post_processing->postprocess_shader) {
		glDeleteProgram(post_processing->postprocess_shader);
		post_processing->postprocess_shader = 0;
	}
	if (post_processing->bloom_prefilter_shader) {
		glDeleteProgram(post_processing->bloom_prefilter_shader);
		post_processing->bloom_prefilter_shader = 0;
	}
	if (post_processing->bloom_downsample_shader) {
		glDeleteProgram(post_processing->bloom_downsample_shader);
		post_processing->bloom_downsample_shader = 0;
	}
	if (post_processing->bloom_upsample_shader) {
		glDeleteProgram(post_processing->bloom_upsample_shader);
		post_processing->bloom_upsample_shader = 0;
	}
	destroy_bloom_resources(post_processing);

	LOG_INFO("suckless-ogl.postprocess", "Post-processing cleaned up");
}

void postprocess_resize(PostProcess* post_processing, int width, int height)
{
	if (post_processing->width == width &&
	    post_processing->height == height) {
		return;
	}

	post_processing->width = width;
	post_processing->height = height;

	/* Recréer le framebuffer avec les nouvelles dimensions */
	destroy_framebuffer(post_processing);
	if (!create_framebuffer(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize framebuffer");
	}

	destroy_bloom_resources(post_processing);
	if (!create_bloom_resources(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize bloom resources");
	}

	LOG_INFO("suckless-ogl.postprocess", "Resized to %dx%d", width, height);
}

void postprocess_enable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects |= effect;
}

void postprocess_disable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects &= ~effect;
}

void postprocess_toggle(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects ^= effect;
}

int postprocess_is_enabled(PostProcess* post_processing,
                           PostProcessEffect effect)
{
	return (post_processing->active_effects & effect) != 0;
}

void postprocess_set_vignette(PostProcess* post_processing, float intensity,
                              float extent)
{
	post_processing->vignette.intensity = intensity;
	post_processing->vignette.extent = extent;
}

void postprocess_set_grain(PostProcess* post_processing, float intensity)
{
	post_processing->grain.intensity = intensity;
}

void postprocess_set_exposure(PostProcess* post_processing, float exposure)
{
	post_processing->exposure.exposure = exposure;
}

void postprocess_set_chrom_abbr(PostProcess* post_processing, float strength)
{
	post_processing->chrom_abbr.strength = strength;
}

void postprocess_set_color_grading(PostProcess* post_processing,
                                   float saturation, float contrast,
                                   float gamma, float gain, float offset)
{
	post_processing->color_grading.saturation = saturation;
	post_processing->color_grading.contrast = contrast;
	post_processing->color_grading.gamma = gamma;
	post_processing->color_grading.gain = gain;
	post_processing->color_grading.gain = gain;
	post_processing->color_grading.offset = offset;
}

void postprocess_set_bloom(PostProcess* post_processing, float intensity,
                           float threshold, float soft_threshold)
{
	post_processing->bloom.intensity = intensity;
	post_processing->bloom.threshold = threshold;
	post_processing->bloom.soft_threshold = soft_threshold;
}

void postprocess_set_dof(PostProcess* post_processing, float focal_distance,
                         float focal_range, float bokeh_scale)
{
	post_processing->dof.focal_distance = focal_distance;
	post_processing->dof.focal_range = focal_range;
	post_processing->dof.bokeh_scale = bokeh_scale;
}

void postprocess_set_grading_ue_default(PostProcess* post_processing)
{
	/* * Valeurs par défaut d'Unreal Engine (Section "Global").
	 * Le "look" UE vient de l'application de ces valeurs neutres
	 * combinées à la courbe de tone mapping ACES dans le shader.
	 */
	post_processing->color_grading.saturation =
	    1.0F;                                       /* Pas de changement */
	post_processing->color_grading.contrast = 1.0F; /* Pas de changement */
	post_processing->color_grading.gamma = 1.0F;    /* Pas de changement */
	post_processing->color_grading.gain = 1.0F;     /* Pas de changement */
	post_processing->color_grading.offset = 0.0F;   /* Pas de changement */

	/* On s'assure que l'effet est activé pour passer dans le pipeline ACES
	 */
	postprocess_enable(post_processing, POSTFX_COLOR_GRADING);
}

void postprocess_apply_preset(PostProcess* post_processing,
                              const PostProcessPreset* preset)
{
	post_processing->active_effects = preset->active_effects;
	post_processing->vignette = preset->vignette;
	post_processing->grain = preset->grain;
	post_processing->exposure = preset->exposure;
	post_processing->chrom_abbr = preset->chrom_abbr;
	post_processing->exposure = preset->exposure;
	post_processing->chrom_abbr = preset->chrom_abbr;
	post_processing->color_grading = preset->color_grading;
	post_processing->color_grading = preset->color_grading;
	post_processing->bloom = preset->bloom;
	post_processing->dof = preset->dof;
}

void postprocess_begin(PostProcess* post_processing)
{
	/* Rendre dans notre framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->scene_fbo);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void postprocess_end(PostProcess* post_processing)
{
	/* Générer le bloom (si activé) avant de binder le framebuffer par
	 * défaut */
	render_bloom(post_processing);

	/* Retour au framebuffer par défaut */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Désactiver le depth test pour le quad */
	glDisable(GL_DEPTH_TEST);

	/* Utiliser le shader de post-processing */
	glUseProgram(post_processing->postprocess_shader);

	/* Bind la texture de la scène */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "screenTexture"),
	            0);

	/* Bind la texture de Bloom */
	glActiveTexture(GL_TEXTURE1);
	if (postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->bloom_mips[0].texture);
	} else {
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "bloomTexture"),
	            1);

	/* Bind la texture de Profondeur (pour le DoF) */
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_depth_tex);
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "depthTexture"),
	            2);

	/* Envoyer les flags d'effets actifs */
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableVignette"),
	            postprocess_is_enabled(post_processing, POSTFX_VIGNETTE));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableGrain"),
	            postprocess_is_enabled(post_processing, POSTFX_GRAIN));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableExposure"),
	            postprocess_is_enabled(post_processing, POSTFX_EXPOSURE));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableChromAbbr"),
	            postprocess_is_enabled(post_processing, POSTFX_CHROM_ABBR));
	glUniform1i(
	    glGetUniformLocation(post_processing->postprocess_shader,
	                         "enableColorGrading"),
	    postprocess_is_enabled(post_processing, POSTFX_COLOR_GRADING));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableBloom"),
	            postprocess_is_enabled(post_processing, POSTFX_BLOOM));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableDoF"),
	            postprocess_is_enabled(post_processing, POSTFX_DOF));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableDoFDebug"),
	            postprocess_is_enabled(post_processing, POSTFX_DOF_DEBUG));

	/* Envoyer les paramètres des effets */
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "vignetteIntensity"),
	            post_processing->vignette.intensity);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "vignetteExtent"),
	            post_processing->vignette.extent);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "grainIntensity"),
	            post_processing->grain.intensity);
	glUniform1f(
	    glGetUniformLocation(post_processing->postprocess_shader, "time"),
	    post_processing->time);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "exposure"),
	            post_processing->exposure.exposure);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "chromAbbrStrength"),
	            post_processing->chrom_abbr.strength);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "bloomIntensity"),
	            post_processing->bloom.intensity);

	/* Paramètres DoF */
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "dofFocalDistance"),
	            post_processing->dof.focal_distance);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "dofFocalRange"),
	            post_processing->dof.focal_range);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "dofBokehScale"),
	            post_processing->dof.bokeh_scale);

	/* Paramètres Color Grading */
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradSaturation"),
	            post_processing->color_grading.saturation);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradContrast"),
	            post_processing->color_grading.contrast);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradGamma"),
	            post_processing->color_grading.gamma);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradGain"),
	            post_processing->color_grading.gain);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradOffset"),
	            post_processing->color_grading.offset);

	/* Dessiner le quad */
	glBindVertexArray(post_processing->screen_quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	glBindVertexArray(0);

	/* Réactiver le depth test */
	glEnable(GL_DEPTH_TEST);
}

void postprocess_update_time(PostProcess* post_processing, float delta_time)
{
	post_processing->time += delta_time;
}

/* Fonctions privées */

static int create_framebuffer(PostProcess* post_processing)
{
	/* Créer le framebuffer */
	glGenFramebuffers(1, &post_processing->scene_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->scene_fbo);

	/* Créer la texture de couleur (HDR) */
	glGenTextures(1, &post_processing->scene_color_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, post_processing->width,
	             post_processing->height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, post_processing->scene_color_tex,
	                       0);

	/* Créer la texture de profondeur (D32F pour précision max) */
	glGenTextures(1, &post_processing->scene_depth_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_depth_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
	             post_processing->width, post_processing->height, 0,
	             GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
	                       GL_TEXTURE_2D, post_processing->scene_depth_tex,
	                       0);

	/* Vérifier que le framebuffer est complet */
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Framebuffer incomplete!");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 1;
}

static int create_screen_quad(PostProcess* post_processing)
{
	glGenVertexArrays(1, &post_processing->screen_quad_vao);
	glGenBuffers(1, &post_processing->screen_quad_vbo);

	glBindVertexArray(post_processing->screen_quad_vao);
	glBindBuffer(GL_ARRAY_BUFFER, post_processing->screen_quad_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(screen_quad_vertices),
	             screen_quad_vertices, GL_STATIC_DRAW);

	/* Position */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
	                      (void*)0);

	/* TexCoords */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
	                      BUFFER_OFFSET(2 * sizeof(float)));

	glBindVertexArray(0);

	return 1;
}

static void destroy_framebuffer(PostProcess* post_processing)
{
	if (post_processing->scene_fbo) {
		glDeleteFramebuffers(1, &post_processing->scene_fbo);
		post_processing->scene_fbo = 0;
	}
	if (post_processing->scene_color_tex) {
		glDeleteTextures(1, &post_processing->scene_color_tex);
		post_processing->scene_color_tex = 0;
	}
	if (post_processing->scene_depth_tex) {
		glDeleteTextures(1, &post_processing->scene_depth_tex);
		post_processing->scene_depth_tex = 0;
	}
}

static void destroy_screen_quad(PostProcess* post_processing)
{
	if (post_processing->screen_quad_vao) {
		glDeleteVertexArrays(1, &post_processing->screen_quad_vao);
		post_processing->screen_quad_vao = 0;
	}
	if (post_processing->screen_quad_vbo) {
		glDeleteBuffers(1, &post_processing->screen_quad_vbo);
		post_processing->screen_quad_vbo = 0;
	}
}

static int create_bloom_resources(PostProcess* post_processing)
{
	glGenFramebuffers(1, &post_processing->bloom_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->bloom_fbo);

	int width = post_processing->width;
	int height = post_processing->height;

	for (int i = 0; i < BLOOM_MIP_LEVELS; i++) {
		/* Chaque niveau est la moitié du précédent */
		width /= 2;
		height /= 2;
		if (width < 1) {
			width = 1;
		}
		if (height < 1) {
			height = 1;
		}

		post_processing->bloom_mips[i].width = width;
		post_processing->bloom_mips[i].height = height;

		glGenTextures(1, &post_processing->bloom_mips[i].texture);
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->bloom_mips[i].texture);
		/* R11G11B10F is good for HDR and saves memory vs RGBA16F */
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, width, height,
		             0, GL_RGB, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		                GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		                GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
		                GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
		                GL_CLAMP_TO_EDGE);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 1;
}

static void destroy_bloom_resources(PostProcess* post_processing)
{
	if (post_processing->bloom_fbo) {
		glDeleteFramebuffers(1, &post_processing->bloom_fbo);
		post_processing->bloom_fbo = 0;
	}

	for (int i = 0; i < BLOOM_MIP_LEVELS; i++) {
		if (post_processing->bloom_mips[i].texture) {
			glDeleteTextures(
			    1, &post_processing->bloom_mips[i].texture);
			post_processing->bloom_mips[i].texture = 0;
		}
	}
}

static void render_bloom(PostProcess* post_processing)
{
	if (!postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->bloom_fbo);
	glDisable(GL_DEPTH_TEST);

	/* 1. Prefilter Pass: Extract bright parts from Scene -> Mip 0 */
	glUseProgram(post_processing->bloom_prefilter_shader);
	glUniform1f(glGetUniformLocation(
	                post_processing->bloom_prefilter_shader, "threshold"),
	            post_processing->bloom.threshold);
	glUniform1f(glGetUniformLocation(
	                post_processing->bloom_prefilter_shader, "knee"),
	            post_processing->bloom.soft_threshold);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	glUniform1i(glGetUniformLocation(
	                post_processing->bloom_prefilter_shader, "srcTexture"),
	            0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D,
	                       post_processing->bloom_mips[0].texture, 0);
	glViewport(0, 0, post_processing->bloom_mips[0].width,
	           post_processing->bloom_mips[0].height);

	glBindVertexArray(post_processing->screen_quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

	/* 2. Downsample Loop */
	glUseProgram(post_processing->bloom_downsample_shader);
	glUniform1i(glGetUniformLocation(
	                post_processing->bloom_downsample_shader, "srcTexture"),
	            0);

	for (int i = 0; i < BLOOM_MIP_LEVELS - 1; i++) {
		const BloomMip* mip_src = &post_processing->bloom_mips[i];
		const BloomMip* mip_dst = &post_processing->bloom_mips[i + 1];

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mip_src->texture);
		glUniform2f(glGetUniformLocation(
		                post_processing->bloom_downsample_shader,
		                "srcResolution"),
		            (float)mip_src->width, (float)mip_src->height);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_2D, mip_dst->texture, 0);
		glViewport(0, 0, mip_dst->width, mip_dst->height);

		glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	}

	/* 3. Upsample Loop with Blending */
	glUseProgram(post_processing->bloom_upsample_shader);
	glUniform1i(glGetUniformLocation(post_processing->bloom_upsample_shader,
	                                 "srcTexture"),
	            0);
	glUniform1f(glGetUniformLocation(post_processing->bloom_upsample_shader,
	                                 "filterRadius"),
	            post_processing->bloom.radius);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);

	for (int i = BLOOM_MIP_LEVELS - 2; i >= 0; i--) {
		const BloomMip* mip_src = &post_processing->bloom_mips[i + 1];
		const BloomMip* mip_dst = &post_processing->bloom_mips[i];

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mip_src->texture);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_2D, mip_dst->texture, 0);
		glViewport(0, 0, mip_dst->width, mip_dst->height);

		glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	}

	glDisable(GL_BLEND);
	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, post_processing->width, post_processing->height);
}