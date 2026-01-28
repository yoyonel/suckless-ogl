#version 450 core

layout(location = 0) in vec3 in_position;  // Quad vertex in local space (+-0.5)
layout(location = 1) in vec3 in_normal;    // Synchronized slot (unused here)

// Per-instance attributes (same slots as mesh rendering for compatibility)
layout(location = 2) in mat4 i_model;   // Instance Model Matrix
layout(location = 6) in vec3 i_albedo;  // Instance Albedo
layout(location = 7) in vec3 i_pbr;  // Instance PBR (Metallic, Roughness, AO)

out vec3 WorldPos;       // Billboard Fragment World Position
out vec3 Normal;         // Synchronized (unused, set to cam vector)
out vec3 SphereCenter;   // Center of the sphere in World Space
out float SphereRadius;  // Radius of the sphere
out vec3 Albedo;
out float Metallic;
out float Roughness;
out float AO;

out vec4 CurrentClipPos;
out vec4 PreviousClipPos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 previousViewProj;  // Kept for interface compatibility, maybe
                                // unused for billboards initially

// We need to calculate the billboard size to bound the sphere
// The sphere is at i_model[3].xyz
// The scale is extracts from i_model columns lengths. Assuming uniform scale
// for spheres.

void main()
{
	// Extract scale from model matrix (assuming uniform scale)
	float scaleX = length(vec3(i_model[0]));
	float scaleY = length(vec3(i_model[1]));
	float scaleZ = length(vec3(i_model[2]));
	float maxScale = max(scaleX, max(scaleY, scaleZ));

	SphereRadius = maxScale * 0.5;  // Model is usually diameter 1 or radius
	                                // 1? Icosphere is radius 1?
	// Let's check icosphere generation. It uses radius 1. So if model scale
	// is 1, radius is 1. Since we want diameter 1 to be scale 1 usually,
	// wait. Icosphere vertices are on unit circle (~1.0). So radius is 1.0.
	// If I scale by 1.0, the object is radius 1.0.
	// Wait, typical "unit sphere" is radius 1 or radius 0.5 (diameter 1)?
	// The existing code uses radius 1.0 (X=0.52... Z=0.85...). distance is
	// sqrt(0.52^2 + 0.85^2) ~= 1. So existing spheres have Radius = Scale.

	SphereRadius = maxScale;
	SphereCenter = vec3(i_model[3]);

	// Billboard calculation
	// We want the quad to always face the camera, but centered at
	// SphereCenter. Standard billboard technique: Camera Right =
	// vec3(view[0][0], view[1][0], view[2][0]) Camera Up    =
	// vec3(view[0][1], view[1][1], view[2][1])

	vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 camUp = vec3(view[0][1], view[1][1], view[2][1]);

	// Calculate world position of this vertex (in_position is like (-0.5,
	// -0.5) to (0.5, 0.5)) We need to scale the quad to cover the sphere.
	// Radius * 2.0 covers the diameter. Better specific slightly larger to
	// avoid precision clipping at edges? Let's stick to exact bounds for
	// now. A sphere of radius R needs a quad of side 2R. If in_position is
	// in range [-0.5, 0.5], sides are length 1.0. So we multiply by
	// SphereRadius * 2.0.

	// Add a small padding (1.5x) to ensure the quad covers the sphere even
	// in perspective projection and to avoid harsh clipping at the edges.
	float quadSize = SphereRadius * 2.0 * 1.5;

	WorldPos = SphereCenter + camRight * in_position.x * quadSize +
	           camUp * in_position.y * quadSize;

	Albedo = i_albedo;
	Metallic = i_pbr.x;
	Roughness = i_pbr.y;
	AO = i_pbr.z;

	// Synchronize Normal output (arbitrary vector for billboards)
	Normal = -vec3(view[0][2], view[1][2], view[2][2]);

	CurrentClipPos = projection * view * vec4(WorldPos, 1.0);
	PreviousClipPos = previousViewProj * vec4(WorldPos, 1.0);

	gl_Position = CurrentClipPos;
}
