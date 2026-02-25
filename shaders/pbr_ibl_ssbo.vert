#version 440 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

/* SSBO avec les données d'instances */
struct InstanceData {
	mat4 model;
	vec3 albedo;
	float metallic;
	float roughness;
	float ao;
	float _padding[2];
};

layout(std430, binding = 0) readonly buffer InstanceBuffer
{
	InstanceData instances[];
};

/* Uniforms globaux */
uniform mat4 projection;
uniform mat4 view;
uniform mat4 previousViewProj;

/* Outputs vers le fragment shader */
out vec3 WorldPos;
out vec3 Normal;
out vec3 Albedo;
out float Metallic;
out float Roughness;
out float AO;

out vec4 CurrentClipPos;
out vec4 PreviousClipPos;

void main()
{
	InstanceData inst = instances[gl_InstanceID];

	vec4 worldPos = inst.model * vec4(aPos, 1.0);
	WorldPos = worldPos.xyz;

	/* CORRECTION : Pour une sphère unitaire, la normale EST la position
	 * normalisée */
	/* On applique juste la rotation de la matrice model (pas de scale dans
	 * notre cas) */
	Normal = normalize(mat3(inst.model) * aNormal);

	/* Passage des propriétés matériau */
	Albedo = inst.albedo;
	Metallic = inst.metallic;
	Roughness = inst.roughness;
	AO = inst.ao;

	CurrentClipPos = projection * view * worldPos;
	PreviousClipPos = previousViewProj * worldPos;

	gl_Position = CurrentClipPos;
}
