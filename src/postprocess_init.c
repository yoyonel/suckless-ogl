#include "app.h"
#include "app_settings.h"
#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "effects/fx_lut3d.h"
#include "effects/fx_lut_viz.h"
#include "effects/fx_motion_blur.h"
#include "log.h"
#include "platform/platform_utils.h"
#include "postprocess_internal.h"
#include "render_utils.h"

int pp_create_framebuffer(PostProcess* post_processing)
{
	/* Ensure Unit 0 is active for initial texture setup */
	glActiveTexture(GL_TEXTURE0);

	/* Créer le framebuffer */
	glGenFramebuffers(1, &post_processing->gpu.scene_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->gpu.scene_fbo);

	/* Créer la texture de couleur (HDR) */
	post_processing->gpu.scene_color_tex = render_utils_create_texture_2d(
	    post_processing->width, post_processing->height, GL_RGBA16F, 1,
	    "Scene Color (HDR)");
	glBindTexture(GL_TEXTURE_2D, post_processing->gpu.scene_color_tex);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D,
	                       post_processing->gpu.scene_color_tex, 0);

	/* Créer la texture de vélocité (GL_RG16F) */
	post_processing->gpu.velocity_tex = render_utils_create_texture_2d(
	    post_processing->width, post_processing->height, GL_RG16F, 1,
	    "Velocity Buffer");
	glBindTexture(GL_TEXTURE_2D, post_processing->gpu.velocity_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
	                       GL_TEXTURE_2D, post_processing->gpu.velocity_tex,
	                       0);

	/* Configurer les buffers de rendu (MRT) */
	GLenum drawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
	glDrawBuffers(2, drawBuffers);

	/* Créer la texture de profondeur (D32F_S8 pour précision max + stencil)
	 */
	post_processing->gpu.scene_depth_tex = render_utils_create_texture_2d(
	    post_processing->width, post_processing->height,
	    GL_DEPTH32F_STENCIL8, 1, "Scene Depth (D32F_S8)");
	glBindTexture(GL_TEXTURE_2D, post_processing->gpu.scene_depth_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
	                       GL_TEXTURE_2D,
	                       post_processing->gpu.scene_depth_tex, 0);

	/* Créer une vue Texture View pour accéder au Stencil uniquement */
	glGenTextures(1, &post_processing->gpu.scene_stencil_view);
	glTextureView(post_processing->gpu.scene_stencil_view, GL_TEXTURE_2D,
	              post_processing->gpu.scene_depth_tex,
	              GL_DEPTH32F_STENCIL8, 0, 1, 0, 1);
	glObjectLabel(GL_TEXTURE, post_processing->gpu.scene_stencil_view, -1,
	              "Scene Stencil View");
	glBindTexture(GL_TEXTURE_2D, post_processing->gpu.scene_stencil_view);
	/* Mode Stencil Index: Permet de lire le canal Stencil (uint) via
	 * usampler2D */
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE,
	                GL_STENCIL_INDEX);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	return render_utils_check_framebuffer("PostProcess Scene FBO");
}

int postprocess_init(PostProcess* post_processing,
                     GPUProfiler* external_profiler, int width, int height)
{
	*post_processing = (PostProcess){0};

	post_processing->gpu_profiler = external_profiler;

	post_processing->width = width;
	post_processing->height = height;
	post_processing->time = 0.0F;
	post_processing->shaders.is_optimized = false;
	post_processing->ubo_dirty = true;
	post_processing->shaders.compiled_flags = ~0U;

	post_processing->shaders.shader_cache_count = 0;
	post_processing->banding_preset_idx = 0;

	/* Paramètres par défaut */
	post_processing->vignette.intensity = DEFAULT_VIGNETTE_INTENSITY;
	post_processing->vignette.smoothness = DEFAULT_VIGNETTE_SMOOTHNESS;
	post_processing->vignette.roundness = DEFAULT_VIGNETTE_ROUNDNESS;
	post_processing->grain.intensity = DEFAULT_GRAIN_INTENSITY;
	post_processing->grain.intensity_shadows = 1.0F;
	post_processing->grain.intensity_midtones = 1.0F;
	post_processing->grain.intensity_highlights = 1.0F;
	post_processing->grain.shadows_max = DEFAULT_GRAIN_SHADOWS_MAX;
	post_processing->grain.highlights_min = DEFAULT_GRAIN_HIGHLIGHTS_MIN;
	post_processing->grain.texel_size = DEFAULT_GRAIN_TEXEL_SIZE;

	post_processing->exposure.exposure = DEFAULT_EXPOSURE;
	post_processing->chrom_abbr.strength = DEFAULT_CHROM_ABBR_STRENGTH;

	/* Bloom defaults (Off) */
	post_processing->bloom.intensity = DEFAULT_BLOOM_INTENSITY;
	post_processing->bloom.threshold = DEFAULT_BLOOM_THRESHOLD;
	post_processing->bloom.soft_threshold = DEFAULT_BLOOM_SOFT_THRESHOLD;
	post_processing->bloom.radius = DEFAULT_BLOOM_RADIUS;

	/* White Balance Defaults */
	post_processing->white_balance.temperature = DEFAULT_WB_TEMP;
	post_processing->white_balance.tint = DEFAULT_WB_TINT;

	/* Tonemapper Defaults */
	post_processing->tonemapper.slope = DEFAULT_FILMIC_SLOPE;
	post_processing->tonemapper.toe = DEFAULT_FILMIC_TOE;
	post_processing->tonemapper.shoulder = DEFAULT_FILMIC_SHOULDER;
	post_processing->tonemapper.black_clip = DEFAULT_FILMIC_BLACK_CLIP;
	post_processing->tonemapper.white_clip = DEFAULT_FILMIC_WHITE_CLIP;

	/* Initialisation Color Grading (Neutre) */
	post_processing->color_grading.saturation = 1.0F;
	post_processing->color_grading.contrast = 1.0F;
	post_processing->color_grading.gamma = 1.0F;
	post_processing->color_grading.gain = 1.0F;
	post_processing->color_grading.offset = 0.0F;

	/* Initialisation Auto Exposure (Stabilisé) */
	post_processing->auto_exposure.min_luminance =
	    EXPOSURE_MIN_LUM; /* Limite le boost max à x2 (1.0 / 0.5) */
	post_processing->auto_exposure.max_luminance = EXPOSURE_DEFAULT_MAX_LUM;
	post_processing->auto_exposure.speed_up = EXPOSURE_SPEED_UP;
	post_processing->auto_exposure.speed_down = EXPOSURE_SPEED_DOWN;
	post_processing->auto_exposure.key_value = EXPOSURE_DEFAULT_KEY_VALUE;
	post_processing->readback.current_exposure = 1.0F;
	post_processing->readback.auto_threshold = 1.0F;

	/* Initialisation Motion Blur */
	if (!fx_motion_blur_init(&post_processing->motion_blur_fx,
	                         &post_processing->motion_blur)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create motion blur resources");
		/* On continue quand même */
	}
	fx_motion_blur_resize(&post_processing->motion_blur_fx,
	                      post_processing->width, post_processing->height);

	/* Initialisation LUT Viz */
	if (fx_lut_viz_init(&post_processing->lut_viz_fx) != 0) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create LUT viz resources");
		/* On continue quand même */
	}

	/* Initialisation FXAA */
	post_processing->fxaa.subpix = DEFAULT_FXAA_SUBPIX;
	post_processing->fxaa.edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD;
	post_processing->fxaa.edge_threshold_min =
	    DEFAULT_FXAA_EDGE_THRESHOLD_MIN;

	/* Initialize Exposure PBOs */
	glGenBuffers(2, post_processing->readback.exposure_pbo);
	for (int i = 0; i < 2; i++) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER,
		             post_processing->readback.exposure_pbo[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(float), NULL,
		             GL_STREAM_READ);
		post_processing->readback.exposure_sync[i] = NULL;
	}

	/* Initialize Histogram PBOs (64x64 floats) */
	glGenBuffers(2, post_processing->readback.histogram_pbo);
	for (int i = 0; i < 2; i++) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER,
		             post_processing->readback.histogram_pbo[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER,
		             (GLsizeiptr)(LUM_HISTOGRAM_SIZE * sizeof(float)),
		             NULL, GL_STREAM_READ);
		post_processing->readback.histogram_sync[i] = NULL;
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	/* Initialisation Banding */
	post_processing->banding.mode = BANDING_MODE_LINEAR;
	post_processing->banding.levels = DEFAULT_BANDING_LEVELS;
	post_processing->banding.dither_strength = 0.0F;
	post_processing->banding.perceptual_gamma = 1.0F;
	post_processing->banding.channel_levels[0] = DEFAULT_BANDING_LEVELS;
	post_processing->banding.channel_levels[1] = DEFAULT_BANDING_LEVELS;
	post_processing->banding.channel_levels[2] = DEFAULT_BANDING_LEVELS;

	/* Initialisation Fog (Off by default) */
	post_processing->fog.density = DEFAULT_FOG_DENSITY;
	post_processing->fog.start = DEFAULT_FOG_START;
	post_processing->fog.height_falloff = DEFAULT_FOG_HEIGHT_FALLOFF;
	post_processing->fog.color[0] = DEFAULT_FOG_COLOR_R;
	post_processing->fog.color[1] = DEFAULT_FOG_COLOR_G;
	post_processing->fog.color[2] = DEFAULT_FOG_COLOR_B;

	/* DoF defaults */
	post_processing->dof.focal_distance = DEFAULT_DOF_FOCAL_DISTANCE;
	post_processing->dof.focal_range = DEFAULT_DOF_FOCAL_RANGE;
	post_processing->dof.bokeh_scale = DEFAULT_DOF_BOKEH_SCALE;
	post_processing->dof.anamorphic_ratio = DEFAULT_DOF_ANAMORPHIC_RATIO;

	/* Effets par défaut définis dans postprocess.h */
	post_processing->active_effects = DEFAULT_ACTIVE_EFFECTS;

	/* Créer le framebuffer */
	if (!pp_create_framebuffer(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create framebuffer");
		return 0;
	}

	/* Créer les ressources Bloom */
	if (!fx_bloom_init(&post_processing->bloom_fx, post_processing->width,
	                   post_processing->height)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create bloom resources");
		pp_destroy_framebuffer(post_processing);
		return 0;
	}

	/* Créer le quad plein écran */
	render_utils_create_fullscreen_quad(
	    &post_processing->gpu.screen_quad_vao,
	    &post_processing->gpu.screen_quad_vbo);
	if (post_processing->gpu.screen_quad_vao == 0) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create screen quad");
		pp_destroy_framebuffer(post_processing);
		fx_bloom_cleanup(&post_processing->bloom_fx);
		return 0;
	}

	/* Charger le shader de post-processing (Optimized Mode) */
	postprocess_compile_optimized(post_processing,
	                              post_processing->active_effects);

	/* Initialize UBO */
	glGenBuffers(1, &post_processing->gpu.settings_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, post_processing->gpu.settings_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(PostProcessUBO), NULL,
	             GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0,
	                 post_processing->gpu.settings_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	if (!fx_auto_exposure_init(&post_processing->auto_exposure_fx)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create auto exposure resources");
		pp_destroy_framebuffer(post_processing);
		fx_bloom_cleanup(&post_processing->bloom_fx);
		fx_dof_cleanup(&post_processing->dof_fx);
		pp_destroy_screen_quad(post_processing);
		return 0;
	}

	/* Créer les ressources DoF */
	if (!fx_dof_init(&post_processing->dof_fx)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create dof resources");
		pp_destroy_framebuffer(post_processing);
		fx_bloom_cleanup(&post_processing->bloom_fx);
		pp_destroy_screen_quad(post_processing);
		return 0;
	}
	if (!fx_dof_resize(&post_processing->dof_fx, post_processing->width,
	                   post_processing->height)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize dof resources");
		fx_dof_cleanup(&post_processing->dof_fx);
		pp_destroy_framebuffer(post_processing);
		fx_bloom_cleanup(&post_processing->bloom_fx);
		pp_destroy_screen_quad(post_processing);
		return 0;
	}

	/* Créer les ressources 3D LUT */
	if (fx_lut3d_init(&post_processing->lut3d_fx) != 0) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create 3D LUT resources");
		/* On continue quand même */
	}
	post_processing->lut3d.intensity = 1.0F;

	LOG_INFO("suckless-ogl.postprocess",
	         "Post-processing initialized (%dx%d)", width, height);

	/* Create dummy uint texture for stencil */
	post_processing->gpu.dummy_uint_tex =
	    render_utils_create_texture_2d(1, 1, GL_R8UI, 1, "Dummy Stencil");
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->gpu.dummy_uint_tex);
	uint32_t zero = 0;
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED_INTEGER,
	                GL_UNSIGNED_INT, &zero);
	glBindTexture(GL_TEXTURE_2D, 0);

	return 1;
}

void postprocess_set_dummy_textures(PostProcess* post_processing,
                                    GLuint dummy_black)
{
	post_processing->gpu.dummy_black_tex = dummy_black;
}

void postprocess_resize(PostProcess* post_processing, int width, int height)
{
	/* Ensure dimensions are at least 1 to avoid OpenGL errors (e.g., during
	 * window minimization) */
	if (width < 1) {
		width = 1;
	}
	if (height < 1) {
		height = 1;
	}

	if (post_processing->width == width &&
	    post_processing->height == height) {
		return;
	}

	post_processing->width = width;
	post_processing->height = height;
	post_processing->ubo_dirty = true;

	/* Recréer le framebuffer avec les nouvelles dimensions */
	pp_destroy_framebuffer(post_processing);
	if (!pp_create_framebuffer(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize framebuffer");
	}

	fx_bloom_cleanup(&post_processing->bloom_fx);
	if (!fx_bloom_init(&post_processing->bloom_fx, post_processing->width,
	                   post_processing->height)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize bloom resources");
	}

	fx_dof_resize(&post_processing->dof_fx, post_processing->width,
	              post_processing->height);
	fx_motion_blur_resize(&post_processing->motion_blur_fx,
	                      post_processing->width, post_processing->height);

	/* Final Bridge: Ensure ALL used units are in a valid state.
	 * NVIDIA driver validates units used by the last shader before resize.
	 */
	render_utils_reset_texture_units(GL_TEXTURE0,
	                                 POSTPROCESS_TEX_UNIT_DOF_BLUR + 1,
	                                 post_processing->gpu.dummy_black_tex);

	/* Bind the real velocity texture on unit 4 to prevent sampling errors
	 */
	if (post_processing->gpu.velocity_tex) {
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_VELOCITY);
		glBindTexture(GL_TEXTURE_2D, post_processing->gpu.velocity_tex);
	}

	/* Reset to Unit 0 for subsequent generic bindings */
	glActiveTexture(GL_TEXTURE0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	LOG_INFO("suckless-ogl.postprocess", "Resized to %dx%d", width, height);
}

/* --- Subsystem descriptor (Phase 1: alloc only) --- */

int postprocess_subsys_init(App* app)
{
	app->postprocess =
	    platform_aligned_alloc(sizeof(*app->postprocess), SIMD_ALIGNMENT);
	if (!app->postprocess) {
		return 0;
	}
	*app->postprocess = (PostProcess){0};
	return 1;
}

void postprocess_subsys_cleanup(App* app)
{
	if (app->postprocess) {
		platform_aligned_free(app->postprocess);
		app->postprocess = NULL;
	}
}
