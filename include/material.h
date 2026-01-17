#ifndef MATERIAL_H
#define MATERIAL_H

#include <cglm/cglm.h>

typedef struct {
	char name[64];
	vec3 albedo;
	float metallic;
	float roughness;
} PBRMaterial;

// On peut stocker une liste de presets
typedef struct {
	PBRMaterial* materials;
	int count;
} MaterialLib;

MaterialLib* material_load_presets(const char* path);
void material_free_lib(MaterialLib* lib);

#endif