#ifndef INSTANCED_RENDERING_H
#define INSTANCED_RENDERING_H

#include "gl_common.h"
#include <cglm/cglm.h>

typedef struct {
	mat4 model;
	vec3 albedo;
	float metallic;
	float roughness;
	float ao;
	float padding;
} __attribute__((aligned(SIMD_ALIGNMENT))) SphereInstance;

typedef struct {
	GLuint vao;           // VAO dédié (Mesh + Instances)
	GLuint instance_vbo;  // Stockage des instances sur GPU
	int instance_count;   // Nombre de sphères
} InstancedGroup;

/* Alloue le buffer d'instances sur le GPU */
void instanced_group_init(InstancedGroup* group, const SphereInstance* data,
                          int count);

/* Lie le groupe aux buffers de quand l'icosphere
 * change) */
void instanced_group_bind_mesh(InstancedGroup* group, GLuint vbo, GLuint nbo,
                               GLuint ebo);

void instanced_group_bind_billboard(InstancedGroup* group, GLuint vbo);

void instanced_group_draw(InstancedGroup* group, size_t index_count);

void instanced_group_draw_arrays(InstancedGroup* group, GLenum mode, int first,
                                 int count);

void instanced_group_cleanup(InstancedGroup* group);

#endif
