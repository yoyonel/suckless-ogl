#ifndef MATERIAL_H
#define MATERIAL_H

#include "gl_common.h"
#include <cglm/cglm.h>

#define MAX_MATERIAL_NAME_LENGTH 64

typedef struct {
	char name[MAX_MATERIAL_NAME_LENGTH];
	vec3 albedo;
	float metallic;
	float roughness;
} __attribute__((aligned(SIMD_ALIGNMENT))) PBRMaterial;

// On peut stocker une liste de presets
typedef struct {
	PBRMaterial* materials;
	int count;
} MaterialLib;

MaterialLib* material_load_presets(const char* path);
void material_free_lib(MaterialLib* lib);

#endif
