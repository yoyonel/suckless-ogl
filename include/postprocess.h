#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "effects/fx_motion_blur.h"
#include "gl_common.h"
#include "shader.h"
#include <cglm/cglm.h>
#include <cglm/types.h>

/* Valeurs par défaut - plus subtiles et cinématiques */
#define DEFAULT_VIGNETTE_INTENSITY 0.8F  /* 0.0 - 1.0+ */
#define DEFAULT_VIGNETTE_SMOOTHNESS 0.5F /* 0.0 (Hard) - 1.0 (Soft) */
#define DEFAULT_VIGNETTE_ROUNDNESS 1.0F  /* 0.0 (Rect) - 1.0 (Round) */
#define DEFAULT_GRAIN_INTENSITY 0.02F    /* 0.05 était trop visible */
#define DEFAULT_GRAIN_SHADOWS_MAX 0.09F
#define DEFAULT_GRAIN_HIGHLIGHTS_MIN 0.5F
#define DEFAULT_GRAIN_TEXEL_SIZE 1.0F
#define DEFAULT_EXPOSURE 1.00F
#define DEFAULT_CHROM_ABBR_STRENGTH 0.005F /* x3 pour le rendre visible */
#define DEFAULT_BLOOM_INTENSITY 0.0F
#define DEFAULT_BLOOM_THRESHOLD 1.0F
#define DEFAULT_BLOOM_SOFT_THRESHOLD 0.5F
#define DEFAULT_BLOOM_RADIUS 1.0F

/* DoF defaults */
#define DEFAULT_DOF_FOCAL_DISTANCE 20.0F /* Match default camera distance */
#define DEFAULT_DOF_FOCAL_RANGE 5.0F     /* Narrower range */
#define DEFAULT_DOF_BOKEH_SCALE 10.0F    /* Stronger blur */

/* White Balance Defaults */
#define DEFAULT_WB_TEMP 6500.0F
#define DEFAULT_WB_TINT 0.0F

/* Filmic Defaults (Safe Neutrals) */
#define DEFAULT_FILMIC_SLOPE 1.0F
#define DEFAULT_FILMIC_TOE 0.0F
#define DEFAULT_FILMIC_SHOULDER 0.0F
#define DEFAULT_FILMIC_BLACK_CLIP 0.0F
#define DEFAULT_FILMIC_WHITE_CLIP 0.0F

/* Types d'effets de post-traitement disponibles */
typedef enum {
	POSTFX_VIGNETTE = (1U << 0U),   /* 0x01 */
	POSTFX_GRAIN = (1U << 1U),      /* 0x02 */
	POSTFX_EXPOSURE = (1U << 2U),   /* 0x04 */
	POSTFX_CHROM_ABBR = (1U << 3U), /* 0x08 */
	/* Réservé pour futurs effets */
	POSTFX_BLOOM = (1U << 4U),              /* 0x10 */
	POSTFX_COLOR_GRADING = (1U << 5U),      /* 0x20 */
	POSTFX_DOF = (1U << 6U),                /* 0x40 */
	POSTFX_DOF_DEBUG = (1U << 7U),          /* 0x80 - Debug Visualization */
	POSTFX_AUTO_EXPOSURE = (1U << 8U),      /* 0x100 */
	POSTFX_EXPOSURE_DEBUG = (1U << 9U),     /* 0x200 */
	POSTFX_MOTION_BLUR = (1U << 10U),       /* 0x400 */
	POSTFX_MOTION_BLUR_DEBUG = (1U << 11U), /* 0x800 */
} PostProcessEffect;

/* Structure pour le Color Grading (Style Unreal Engine) */
typedef struct {
	float saturation; /* 0.0 (Gris) - 2.0 (Saturé), Défaut: 1.0 */
	float contrast;   /* 0.0 - 2.0, Défaut: 1.0 */
	float gamma;      /* 0.0 - 2.0, Défaut: 1.0 */
	float gain;       /* 0.0 - 2.0, Défaut: 1.0 */
	float offset;     /* -1.0 - 1.0, Défaut: 0.0 */
} ColorGradingParams;

/* Paramètres pour le vignettage */
typedef struct {
	float intensity;  /* 0.0 - 1.0+ */
	float smoothness; /* 0.0 - 1.0 */
	float roundness;  /* 0.0 - 1.0 */
} VignetteParams;

/* Paramètres pour le grain */
typedef struct {
	float intensity;            /* Global multiplier */
	float intensity_shadows;    /* Multiplier for dark areas */
	float intensity_midtones;   /* Multiplier for mid-tone areas */
	float intensity_highlights; /* Multiplier for bright areas */
	float shadows_max;          /* Luma threshold for shadows (0.0 - 1.0) */
	float highlights_min; /* Luma threshold for highlights (0.0 - 1.0) */
	float texel_size;     /* Grain particle size (scale) */
} GrainParams;

/* Paramètres pour l'exposition
 * NOTE: When POSTFX_AUTO_EXPOSURE is enabled, this manual exposure is IGNORED.
 * Auto-exposure will calculate and apply exposure automatically.
 * Only use manual exposure when auto-exposure is disabled.
 */
typedef struct {
	float exposure; /* 0.5 - 2.0, défaut: 1.0, recommandé: 0.8-1.5 */
} ExposureParams;

/* Paramètres pour l'aberration chromatique */
typedef struct {
	float strength; /* 0.0 - 0.05, défaut: 0.01, recommandé: 0.005-0.02
	                   (visible aux bords) */
} ChromAbberationParams;

/* Paramètres White Balance */
typedef struct {
	float temperature; /* Température de couleur (Kelvin), Défaut: 6500.0 */
	float tint;        /* Teinte (Vert/Magenta), -1.0 à 1.0, Défaut: 0.0 */
} WhiteBalanceParams;

/* Paramètres Filmic Tonemapper (Custom Curve) */
typedef struct {
	float slope;      /* Pente (Contraste), Défaut: 0.91 */
	float toe;        /* Pied (Noirs), Défaut: 0.53 */
	float shoulder;   /* Épaule (Blancs), Défaut: 0.23 */
	float black_clip; /* Coupure des noirs, Défaut: 0.0 */
	float white_clip; /* Coupure des blancs, Défaut: 0.035 */
} TonemapParams;

#define BLOOM_MIP_LEVELS 5

/**
 * @brief Uniform Buffer Object structure for post-processing settings.
 * Aligned for std140 layout in GLSL.
 */
typedef struct {
	uint32_t active_effects;
	float time;
	float _pad0[2];

	/* Vignette */
	float vignette_intensity;
	float vignette_smoothness;
	float vignette_roundness;
	float _pad1;

	/* Grain */
	float grain_intensity;
	float grain_intensity_shadows;
	float grain_intensity_midtones;
	float grain_intensity_highlights;
	float grain_shadows_max;
	float grain_highlights_min;
	float grain_texel_size;
	float _pad2;

	/* Exposure */
	float exposure_manual;
	float _pad3[3];

	/* Chrom Abbr */
	float chrom_abbr_strength;
	float _pad4[3];

	/* White Balance */
	float wb_temperature;
	float wb_tint;
	float _pad5[2];

	/* Color Grading */
	float grading_saturation;
	float grading_contrast;
	float grading_gamma;
	float grading_gain;
	float grading_offset;
	float _pad6[3];

	/* Tonemapper */
	float tonemap_slope;
	float tonemap_toe;
	float tonemap_shoulder;
	float tonemap_black_clip;
	float tonemap_white_clip;
	float _pad7[3];

	/* Bloom */
	float bloom_intensity;
	float bloom_threshold;
	float bloom_soft_threshold;
	float bloom_radius;

	/* DoF */
	float dof_focal_distance;
	float dof_focal_range;
	float dof_bokeh_scale;
	float _pad8;

	/* Motion Blur */
	float mb_intensity;
	float mb_max_velocity;
	int32_t mb_samples;
	float _pad9;
} PostProcessUBO;

/* Structure principale du système de post-processing */
typedef struct PostProcess {
	/* FBO principal et textures */
	GLuint scene_fbo;
	GLuint scene_color_tex; /* HDr (GL_RGBA16F) */
	GLuint velocity_tex;    /* Velocity Buffer (GL_RG16F) */
	GLuint scene_depth_tex; /* Depth (GL_DEPTH_COMPONENT32F) */

	/* Bloom Resources */
	BloomFX bloom_fx;

	/* DoF Resources */
	DoFFX dof_fx;

	/* Auto Exposure Resources */
	AutoExposureFX auto_exposure_fx;

	/* Motion Blur Resources */
	MotionBlurFX motion_blur_fx;

	/* Quad plein écran */
	GLuint screen_quad_vao;
	GLuint screen_quad_vbo;

	/* UBO for settings */
	GLuint settings_ubo;

	/* Shaders */
	Shader* postprocess_shader;  /* Shader combinant tous les effets */
	Shader* tile_max_shader;     /* Compute Shader: Tile Max Velocity */
	Shader* neighbor_max_shader; /* Compute Shader: Neighbor Max
	                                Velocity */

	/* Dimensions */
	int width;
	int height;

	/* Pipeline Settings (Effets actifs) */
	unsigned int active_effects;

	/* Paramètres Effets */
	VignetteParams vignette;
	GrainParams grain;
	ExposureParams exposure;
	ChromAbberationParams chrom_abbr;
	WhiteBalanceParams white_balance; /* [NEW] */
	ColorGradingParams color_grading;
	TonemapParams tonemapper; /* [NEW] */
	BloomParams bloom;
	DoFParams dof;
	AutoExposureParams auto_exposure;

	MotionBlurParams motion_blur;
	/* Temps pour effets animés (grain) */
	float time;
	float delta_time; /* Added needed for time integration */
} PostProcess;

/* Initialisation et nettoyage */
int postprocess_init(PostProcess* post_processing, int width, int height);
void postprocess_cleanup(PostProcess* post_processing);

/* Redimensionnement */
void postprocess_resize(PostProcess* post_processing, int width, int height);

/* Activation/désactivation d'effets */
void postprocess_enable(PostProcess* post_processing, PostProcessEffect effect);
void postprocess_disable(PostProcess* post_processing,
                         PostProcessEffect effect);
void postprocess_toggle(PostProcess* post_processing, PostProcessEffect effect);
int postprocess_is_enabled(PostProcess* post_processing,
                           PostProcessEffect effect);
/* Configuration des paramètres de Color Grading */
void postprocess_set_white_balance(PostProcess* post_processing,
                                   float temperature, float tint);
void postprocess_set_color_grading(PostProcess* post_processing,
                                   float saturation, float contrast,
                                   float gamma, float gain, float offset);
void postprocess_set_tonemapper(PostProcess* post_processing, float slope,
                                float toe, float shoulder, float black_clip,
                                float white_clip);
void postprocess_set_grading_ue_default(PostProcess* post_processing);

/* Configuration des paramètres */
void postprocess_set_vignette(PostProcess* post_processing, float intensity,
                              float smoothness, float roundness);
void postprocess_set_grain(PostProcess* post_processing, float intensity);
void postprocess_set_exposure(PostProcess* post_processing, float exposure);
void postprocess_set_chrom_abbr(PostProcess* post_processing, float strength);
void postprocess_set_bloom(PostProcess* post_processing, float intensity,
                           float threshold, float soft_threshold);
void postprocess_set_dof(PostProcess* post_processing, float focal_distance,
                         float focal_range, float bokeh_scale);
float postprocess_get_exposure(PostProcess* post_processing);
void postprocess_set_auto_exposure(PostProcess* post_processing,
                                   float min_luminance, float max_luminance,
                                   float speed_up, float speed_down,
                                   float key_value);

void postprocess_update_matrices(PostProcess* post_processing, mat4 view_proj);

/* Structure de Preset pour l'application en masse de paramètres */
typedef struct {
	unsigned int active_effects;
	VignetteParams vignette;
	GrainParams grain;
	ExposureParams exposure;
	ChromAbberationParams chrom_abbr;
	WhiteBalanceParams white_balance;
	ColorGradingParams color_grading;
	TonemapParams tonemapper;
	BloomParams bloom;
	DoFParams dof;
} PostProcessPreset;

/* Application de preset */
void postprocess_apply_preset(PostProcess* post_processing,
                              const PostProcessPreset* preset);

/* Rendu */
void postprocess_begin(
    PostProcess* post_processing); /* Commence le rendu dans le FBO */
void postprocess_end(
    PostProcess* post_processing); /* Applique les effets et rend à l'écran */
void postprocess_update_time(PostProcess* post_processing, float delta_time);

#endif /* POSTPROCESS_H */
