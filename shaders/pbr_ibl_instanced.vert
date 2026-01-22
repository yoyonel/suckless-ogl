#version 450 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;

// Attributs d'instance (Vertex Attrib Divisor = 1)
layout (location = 2) in mat4 i_model;     // Emplacement 2, 3, 4, 5
layout (location = 6) in vec3 i_albedo;    // Emplacement 6
layout (location = 7) in vec3 i_pbr;       // Emplacement 7 (x: metallic, y: roughness, z: ao)

out vec3 WorldPos;
out vec3 Normal;
out vec3 Albedo;
out float Metallic;
out float Roughness;
out float AO;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 previousViewProj;

out vec4 CurrentClipPos;
out vec4 PreviousClipPos;

void main() {
    WorldPos = vec3(i_model * vec4(in_position, 1.0));

    // Calcul de la NormalMatrix par instance
    // Si vous n'avez pas de scale non-uniforme, mat3(i_model) suffit pour les performances
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
