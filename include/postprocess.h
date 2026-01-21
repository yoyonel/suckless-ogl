#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "gl_common.h"
#include <cglm/cglm.h>

/* Valeurs par défaut - plus subtiles et cinématiques */
#define DEFAULT_VIGNETTE_INTENSITY 0.03F /* 0.5 était trop fort */
#define DEFAULT_VIGNETTE_EXTENT 0.7F     /* Plus large = effet plus doux */
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
#define DEFAULT_DOF_FOCAL_RANGE 10.0F    /* Wider range */
#define DEFAULT_DOF_BOKEH_SCALE 1.0F     /* Subtle blur */

/* Types d'effets de post-traitement disponibles */
typedef enum {
	POSTFX_VIGNETTE = (1U << 0U),   /* 0x01 */
	POSTFX_GRAIN = (1U << 1U),      /* 0x02 */
	POSTFX_EXPOSURE = (1U << 2U),   /* 0x04 */
	POSTFX_CHROM_ABBR = (1U << 3U), /* 0x08 */
	/* Réservé pour futurs effets */
	POSTFX_BLOOM = (1U << 4U),          /* 0x10 */
	POSTFX_COLOR_GRADING = (1U << 5U),  /* 0x20 */
	POSTFX_DOF = (1U << 6U),            /* 0x40 */
	POSTFX_DOF_DEBUG = (1U << 7U),      /* 0x80 - Debug Visualization */
	POSTFX_AUTO_EXPOSURE = (1U << 8U),  /* 0x100 */
	POSTFX_EXPOSURE_DEBUG = (1U << 9U), /* 0x200 */
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
	float intensity; /* 0.0 - 1.0, défaut: 0.3, recommandé: 0.2-0.4 (subtil)
	                  */
	float extent; /* 0.3 - 0.8, défaut: 0.7, recommandé: 0.6-0.8 (doux) */
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

/* Paramètres pour l'exposition */
typedef struct {
	float exposure; /* 0.5 - 2.0, défaut: 1.0, recommandé: 0.8-1.5 */
} ExposureParams;

/* Paramètres pour l'aberration chromatique */
typedef struct {
	float strength; /* 0.0 - 0.05, défaut: 0.01, recommandé: 0.005-0.02
	                   (visible aux bords) */
} ChromAbberationParams;

/* Paramètres pour le Bloom (Physically Based) */
typedef struct {
	float intensity;      /* Puissance globale (0.0 - 1.0+) */
	float threshold;      /* Seuil de luminance (1.0+) */
	float soft_threshold; /* Genou de transition (0.0 - 1.0) */
	float radius;         /* Rayon du bloom (simulé par # mips) */
} BloomParams;

/* Structure pour un niveau de mip du Bloom */
typedef struct {
	GLuint texture;
	int width;
	int height;
} BloomMip;

/* Paramètres pour le Depth of Field */
typedef struct {
	float focal_distance; /* Distance de mise au point (unités monde) */
	float focal_range; /* Plage de netteté (autour de la distance focale) */
	float bokeh_scale; /* Taille du flou (simule l'ouverture) */
} DoFParams;

/* Paramètres pour l'Auto Exposure (Eye Adaptation) */
typedef struct {
	float min_luminance; /* Luminance min (clamping) */
	float max_luminance; /* Luminance max (clamping) */
	float speed_up; /* Vitesse d'adaptation vers clair (pupille s'ouvre) */
	float speed_down; /* Vitesse d'adaptation vers sombre (pupille se ferme)
	                   */
	float key_value;  /* Target exposure value (middle gray), def: 1.0 */
} AutoExposureParams;

#define BLOOM_MIP_LEVELS 5
#define LUM_DOWNSAMPLE_SIZE 64

/* Structure principale du système de post-processing */
typedef struct {
	/* FBO principal et textures */
	GLuint scene_fbo;
	GLuint scene_color_tex; /* HDr (GL_RGBA16F) */
	GLuint scene_depth_tex; /* Depth (GL_DEPTH_COMPONENT32F) */

	/* Bloom Resources */
	GLuint bloom_fbo; /* FBO partagé pour le blit */
	BloomMip bloom_mips[BLOOM_MIP_LEVELS];

	/* Auto Exposure Resources */
	GLuint lum_downsample_fbo; /* FBO pour downsample luminance */
	GLuint lum_downsample_tex; /* Texture 64x64 Log Luminance */
	GLuint exposure_tex; /* Texture 1x1 R32F (Storage) - Current Exposure */

	/* Quad plein écran */
	GLuint screen_quad_vao;
	GLuint screen_quad_vbo;

	/* Shaders */
	GLuint postprocess_shader; /* Shader combinant tous les effets */
	GLuint bloom_prefilter_shader;
	GLuint bloom_downsample_shader;
	GLuint bloom_upsample_shader;
	GLuint lum_downsample_shader; /* Shader pour Log Lum Downsample */
	GLuint lum_adapt_shader; /* Compute Shader pour moyenne + adaptation */
	GLuint dof_shader;

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
	ColorGradingParams color_grading;
	BloomParams bloom;
	DoFParams dof;
	AutoExposureParams auto_exposure;

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
void postprocess_set_color_grading(PostProcess* post_processing,
                                   float saturation, float contrast,
                                   float gamma, float gain, float offset);
void postprocess_set_grading_ue_default(PostProcess* post_processing);

/* Configuration des paramètres */
void postprocess_set_vignette(PostProcess* post_processing, float intensity,
                              float extent);
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

/* Structure de Preset pour l'application en masse de paramètres */
typedef struct {
	unsigned int active_effects;
	VignetteParams vignette;
	GrainParams grain;
	ExposureParams exposure;
	ChromAbberationParams chrom_abbr;
	ColorGradingParams color_grading;
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