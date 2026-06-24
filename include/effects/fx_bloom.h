#ifndef FX_BLOOM_H
#define FX_BLOOM_H

#include "gl_common.h"
typedef struct Shader Shader;

/* Forward declarations */
struct EffectContext;

enum { BLOOM_MIP_LEVELS = 5 };

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

typedef enum {
	BLOOM_FINAL = 0,
	BLOOM_PREFILTER,
	BLOOM_DOWNSAMPLE,
	BLOOM_UPSAMPLE,
} BloomState;

static char* const bloom_stages[] = {"Final Map", "Prefilter", "Downsample",
                                     "Upsample"};

/* Structure regroupant les ressources graphiques du Bloom */
typedef struct BloomFX {
	Shader* prefilter_shader;
	Shader* downsample_shader;
	Shader* upsample_shader;
	GLuint fbo;
	BloomMip mips[BLOOM_MIP_LEVELS];
	BloomState bloom_step;
	int debug_mip; /* Sub-level for Downsample/Upsample debug */
} BloomFX;

/* Initialisation des ressources Bloom */
int fx_bloom_init(BloomFX* bloom, int width, int height);

/* Libération des ressources */
void fx_bloom_cleanup(BloomFX* bloom);

/* Rendu de l'effet */
void fx_bloom_render(BloomFX* bloom, const BloomParams* params,
                     const struct EffectContext* ctx);

/* Upload des paramètres vers le shader principal */
void fx_bloom_upload_params(Shader* shader, const BloomParams* params);

/* Getters for shared shaders (used by DoF) */
Shader* fx_bloom_get_downsample_shader(BloomFX* bloom);
Shader* fx_bloom_get_upsample_shader(BloomFX* bloom);

#endif /* FX_BLOOM_H */
