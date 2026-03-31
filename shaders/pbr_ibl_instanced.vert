#version 450 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

// Attributs d'instance (Vertex Attrib Divisor = 1)
layout(location = 2) in mat4 i_model;   // Emplacement 2, 3, 4, 5
layout(location = 6) in vec3 i_albedo;  // Emplacement 6
layout(location = 7)
    in vec3 i_pbr;  // Emplacement 7 (x: metallic, y: roughness, z: ao)

layout(location = 0) out vec3 WorldPos;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec3 Albedo;
layout(location = 3) out float Metallic;
layout(location = 4) out float Roughness;
layout(location = 5) out float AO;

layout(location = 0) uniform mat4 projection;
layout(location = 4) uniform mat4 view;
layout(location = 8) uniform mat4 previousViewProj;

layout(location = 6) out vec4 CurrentClipPos;
layout(location = 7) out vec4 PreviousClipPos;

void main()
{
	WorldPos = vec3(i_model * vec4(in_position, 1.0));

	// Calcul de la NormalMatrix par instance
	// Si vous n'avez pas de scale non-uniforme, mat3(i_model) suffit pour
	// les performances
	mat3 normalMatrix = mat3(transpose(inverse(i_model)));
	Normal = normalize(normalMatrix * in_normal);

	Albedo = i_albedo;
	Metallic = i_pbr.x;
	Roughness = i_pbr.y;
	AO = i_pbr.z;

	CurrentClipPos = projection * view * vec4(WorldPos, 1.0);
	PreviousClipPos = previousViewProj * vec4(WorldPos, 1.0);

	gl_Position = CurrentClipPos;
}
