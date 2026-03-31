#version 450 core

layout(location = 0) in vec3 in_position;  // Quad vertex in local space (+-0.5)
layout(location = 1) in vec3 in_normal;    // Unused

// Per-instance data fetched from SSBO via gl_InstanceID (Tier 4)

layout(location = 0) out vec3 WorldPos;  // Point on the billboard plane
layout(location = 1) out vec3 Normal;    // Synchronized (unused)

// Données transmises "flat" (sans interpolation) au Fragment Shader
flat layout(location = 2) out vec3
    SphereCenter;  // Center of the sphere in World Space
flat layout(location = 3) out float SphereRadius;  // Radius of the sphere
flat layout(location = 4) out vec3 Albedo;
flat layout(location = 5) out float Metallic;
flat layout(location = 6) out float Roughness;
flat layout(location = 7) out float AO;

layout(location = 8) out vec4
    CurrentClipPos;  // Used for AA size calculation in Frag
// out vec4 PreviousClipPos;  <-- SUPPRIMÉ : Le calcul vertex est faux pour un
// billboard

/* clang-format off */
@header "billboard_instance_ssbo.glsl"
@header "billboard_ubo.glsl"
@header "projection_utils.glsl"
    /* clang-format on */

    void
    main()
{
	// Fetch instance data from sorted SSBO (no VBO copy needed)
	SphereInstance inst = billboard_instances[gl_InstanceID];

	// 1. Extraction de l'échelle (Rayon)
	float scaleX = length(vec3(inst.model[0]));
	float scaleY = length(vec3(inst.model[1]));
	float scaleZ = length(vec3(inst.model[2]));
	float maxScale = max(scaleX, max(scaleY, scaleZ));

	SphereRadius = maxScale;
	SphereCenter = vec3(inst.model[3]);

	// 2. Calcul de la géométrie du Billboard (Méthode Exacte ou
	// Conservative) Cette fonction remplit clipPos et WorldPos
	vec4 clipPos;
	computeBillboardSphere(in_position, SphereCenter, SphereRadius, view,
	                       projection, clipPos, WorldPos);

	// 3. Transmission des matériaux
	Albedo = inst.albedo;
	Metallic = inst.metallic;
	Roughness = inst.roughness;
	AO = inst.ao;

	// Normale "face caméra" pour le quad (la vraie normale sera calculée
	// par raytracing)
	Normal = -vec3(view[0][2], view[1][2], view[2][2]);

	CurrentClipPos = clipPos;
	gl_Position = CurrentClipPos;
}
