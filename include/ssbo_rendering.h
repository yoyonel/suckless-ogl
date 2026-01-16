#ifndef SSBO_RENDERING_H
#define SSBO_RENDERING_H

#include "gl_common.h"
#include <cglm/types.h>

/**
 * Structure alignée pour le SSBO (std430)
 * Chaque instance occupe exactement 80 bytes
 */
typedef struct {
	mat4 model;        /* 64 bytes (4x vec4) */
	vec3 albedo;       /* 12 bytes */
	float metallic;    /* 4 bytes */
	float roughness;   /* 4 bytes */
	float ao;          /* 4 bytes */
	float _padding[2]; /* 8 bytes - alignment pour 80 bytes total */
} SphereInstanceSSBO;

typedef struct {
	GLuint ssbo;
	GLuint vao;
	int instance_count;
} SSBOGroup;

/**
 * Initialise un groupe SSBO avec les données d'instances
 */
void ssbo_group_init(SSBOGroup* group, const SphereInstanceSSBO* data,
                     int count);

/**
 * Lie la géométrie mesh au VAO du groupe SSBO
 */
void ssbo_group_bind_mesh(SSBOGroup* group, GLuint vbo, GLuint nbo, GLuint ebo);

/**
 * Effectue le rendu instancié via SSBO
 */
void ssbo_group_draw(SSBOGroup* group, size_t index_count);

/**
 * Libère les ressources du groupe SSBO
 */
void ssbo_group_cleanup(SSBOGroup* group);

#endif /* SSBO_RENDERING_H */