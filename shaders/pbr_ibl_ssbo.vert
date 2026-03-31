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
layout(location = 0) uniform mat4 projection;
layout(location = 4) uniform mat4 view;

/* Outputs vers le fragment shader */
layout(location = 0) out vec3 WorldPos;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec3 Albedo;
layout(location = 3) out float Metallic;
layout(location = 4) out float Roughness;
layout(location = 5) out float AO;

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

	gl_Position = projection * view * worldPos;
}
