#version 450 core

layout(location = 0) in vec3 in_position;  // Quad vertex in local space (+-0.5)
layout(location = 1) in vec3 in_normal;    // Unused

// Per-instance attributes
layout(location = 2) in mat4 i_model;   // Instance Model Matrix
layout(location = 6) in vec3 i_albedo;  // Instance Albedo
layout(location = 7) in vec3 i_pbr;  // Instance PBR (Metallic, Roughness, AO)

out vec3 WorldPos;  // Point on the billboard plane
out vec3 Normal;    // Synchronized (unused)

// Données transmises "flat" (sans interpolation) au Fragment Shader
flat out vec3 SphereCenter;   // Center of the sphere in World Space
flat out float SphereRadius;  // Radius of the sphere
flat out vec3 Albedo;
flat out float Metallic;
flat out float Roughness;
flat out float AO;

out vec4 CurrentClipPos;  // Used for AA size calculation in Frag
// out vec4 PreviousClipPos;  <-- SUPPRIMÉ : Le calcul vertex est faux pour un
// billboard

uniform mat4 projection;
uniform mat4 view;
// uniform mat4 previousViewProj; <-- Inutile ici désormais

// Header pour la fonction computeBillboardSphere
@header "projection_utils.glsl"

    void
    main()
{
	// 1. Extraction de l'échelle (Rayon)
	float scaleX = length(vec3(i_model[0]));
	float scaleY = length(vec3(i_model[1]));
	float scaleZ = length(vec3(i_model[2]));
	float maxScale = max(scaleX, max(scaleY, scaleZ));

	SphereRadius = maxScale;
	SphereCenter = vec3(i_model[3]);

	// 2. Calcul de la géométrie du Billboard (Méthode Exacte ou
	// Conservative) Cette fonction remplit clipPos et WorldPos
	vec4 clipPos;
	computeBillboardSphere(in_position, SphereCenter, SphereRadius, view,
	                       projection, clipPos, WorldPos);

	// 3. Transmission des matériaux
	Albedo = i_albedo;
	Metallic = i_pbr.x;
	Roughness = i_pbr.y;
	AO = i_pbr.z;

	// Normale "face caméra" pour le quad (la vraie normale sera calculée
	// par raytracing)
	Normal = -vec3(view[0][2], view[1][2], view[2][2]);

	CurrentClipPos = clipPos;
	gl_Position = CurrentClipPos;
}