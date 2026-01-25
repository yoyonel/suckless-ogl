#ifndef FX_BLOOM_H
#define FX_BLOOM_H

#include "gl_common.h"
#include "shader.h"

/* Forward declaration */
struct PostProcess;

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

/* Structure regroupant les ressources graphiques du Bloom */
typedef struct {
	Shader* prefilter_shader;
	Shader* downsample_shader;
	Shader* upsample_shader;
	GLuint fbo;
	BloomMip mips[BLOOM_MIP_LEVELS];
} BloomFX;

/* Initialisation des ressources Bloom */
int fx_bloom_init(struct PostProcess* post_processing);

/* Libération des ressources */
void fx_bloom_cleanup(struct PostProcess* post_processing);

/* Rendu de l'effet */
void fx_bloom_render(struct PostProcess* post_processing);

/* Upload des paramètres vers le shader principal */
void fx_bloom_upload_params(Shader* shader, const BloomParams* params);

/* Getters for shared shaders */
Shader* fx_bloom_get_downsample_shader(struct PostProcess* post_processing);
Shader* fx_bloom_get_upsample_shader(struct PostProcess* post_processing);

#endif /* FX_BLOOM_H */
