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
// for spheres.

// Helper function to calculate 1D projected bounds (NDC)
void getProjectedBounds(vec2 axis, float radius, float projScale,
                        out float outMin, out float outMax)
{
	float d2 = dot(axis, axis);
	float r2 = radius * radius;

	// Check if we are inside or too close, fallback to full range if needed
	// (handled in main usually)
	if (d2 <= r2) {
		outMin = -1.0;
		outMax = 1.0;
		return;
	}

	float L = sqrt(max(0.0, d2 - r2));

	// Tangent logic to find normal of tangent lines
	// Tangent 1
	float nx1 = (axis.x * L - axis.y * radius) / d2;
	float nz1 = (axis.y * L + axis.x * radius) / d2;

	// Tangent 2
	float nx2 = (axis.x * L + axis.y * radius) / d2;
	float nz2 = (axis.y * L - axis.x * radius) / d2;

	// Project to NDC: x_ndc = (nx / -nz) * projScale
	// Division by -nz because OpenGL looks down -Z
	// HANDLE SINGULARITY: If tangent point is behind camera (nz >= 0),
	// we clamp to infinity in the direction of nx.
	float p1, p2;

	if (nz1 > -0.001)
		p1 = sign(nx1) * 10000.0;
	else
		p1 = projScale * (nx1 / -nz1);

	if (nz2 > -0.001)
		p2 = sign(nx2) * 10000.0;
	else
		p2 = projScale * (nx2 / -nz2);

	outMin = min(p1, p2);
	outMax = max(p1, p2);
}

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

	// Standard billboard vectors (needed for fallback)
	vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 camUp = vec3(view[0][1], view[1][1], view[2][1]);

	// Exact AABB Calculation (Tangent Planes Method)
	vec3 viewPos = (view * vec4(SphereCenter, 1.0)).xyz;
	float distSq = dot(viewPos, viewPos);
	float r2 = SphereRadius * SphereRadius;

	vec4 clipPos;

	// Check if camera is inside the sphere
	if (distSq <= r2 * 1.001) {
		// Inside: Full screen quad ? Or fallback.
		// For a billboard, this might clip awkwardly.
		// We'll set a massive quad in front of the camera.
		clipPos =
		    vec4(in_position.xy * 2.0, 0.0, 1.0);  // Simple fill NDC

		// For WorldPos reconstruction, we need something valid.
		// But if we are inside, standard raytracing logic in frag
		// shader handles it if we pass correct SphereCenter. Here we
		// just want to ensure rasterization covers the screen.
		WorldPos = SphereCenter +
		           camRight * in_position.x * SphereRadius * 100.0 +
		           camUp * in_position.y * SphereRadius * 100.0;
	} else if (viewPos.z > 0.0) {
		// Optimization: Cull spheres that are fully behind the camera
		// plane (and don't contain it) If z > 0, the sphere is behind
		// the eye. Since we are in the 'else' of (dist <= r), we know d
		// > r, so the sphere does not contain the eye. It is invisible.
		// We project it to outside clip space.
		clipPos = vec4(-2.0, -2.0, 0.0, 1.0);
		WorldPos = SphereCenter;
	} else {
		float sx = projection[0][0];
		float sy = projection[1][1];

		float minX, maxX, minY, maxY;
		getProjectedBounds(vec2(viewPos.x, viewPos.z), SphereRadius, sx,
		                   minX, maxX);
		getProjectedBounds(vec2(viewPos.y, viewPos.z), SphereRadius, sy,
		                   minY, maxY);

		// Select NDC coordinates based on quad vertex sign
		float ndc_x = (in_position.x < 0.0) ? minX : maxX;
		float ndc_y = (in_position.y < 0.0) ? minY : maxY;

		// Conservative Depth: Place quad at the sphere's front surface to prevent incorrect Z-culling
		// by intersecting geometry (e.g. walls) before the fragment shader runs.
		// Since we look down -Z, adding radius brings us closer to the camera (0).
		float nearestZ = viewPos.z + SphereRadius;
		// Clamp to ensure we stay in front of the camera (negative Z) even if sphere grazes the plane
		nearestZ = min(nearestZ, -0.01);

		float clipW = -nearestZ;
		float clipZ = projection[2][2] * nearestZ + projection[3][2];
		
		clipPos = vec4(ndc_x * clipW, ndc_y * clipW, clipZ, clipW);
		
		// Reconstruct WorldPos for the Fragment Shader (Ray Origin / Direction)
		// We need the point in World Space that corresponds to this vertex on the billboard plane.
		// NOTE: For raycasting, we ideally want the plane to be at the center (viewPos.z)
		// to minimize distortion, but using nearestZ for the rasterized quad is safer for Z-test.
		// The ray direction calculation depends on WorldPos.
		// If we use nearestZ for WorldPos reconstruction, the ray origin is shifted.
		// Let's stick to the center plane for WorldPos reconstruction to keep the math simple/stable
		// for the ray intersection logic (which usually assumes rays starting from camera).
		// Wait, WorldPos IS the point on the quad. If the quad moves, WorldPos must move.
		
		vec3 vertexViewPos;
		vertexViewPos.z = nearestZ;
		vertexViewPos.x = ndc_x * (-nearestZ) / sx;
		vertexViewPos.y = ndc_y * (-nearestZ) / sy;
		
		vec3 viewOffset = vertexViewPos - viewPos; // viewPos is still center
		vec3 worldOffset = transpose(mat3(view)) * viewOffset;
		
		WorldPos = SphereCenter + worldOffset;
	}

	// Albedo and PBR setup
	Albedo = i_albedo;
	Metallic = i_pbr.x;
	Roughness = i_pbr.y;
	AO = i_pbr.z;

	// Synchronize Normal output
	Normal = -vec3(view[0][2], view[1][2], view[2][2]);

	CurrentClipPos = clipPos;
	PreviousClipPos = previousViewProj * vec4(WorldPos, 1.0);

	gl_Position = CurrentClipPos;
}
