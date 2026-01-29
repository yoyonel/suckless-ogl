#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 VelocityOut;

in vec3 WorldPos;  // Position on the billboard plane
in vec3 Normal;    // Synchronized (unused)
in vec3 SphereCenter;
in float SphereRadius;
in vec3 Albedo;
in float Metallic;
in float Roughness;
in float AO;

in vec4 CurrentClipPos;
in vec4 PreviousClipPos;

uniform vec3 camPos;
uniform sampler2D irradianceMap;
uniform sampler2D prefilterMap;
uniform sampler2D brdfLUT;
uniform int debugMode;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 previousViewProj;

// Include common PBR functions
@header "pbr_functions.glsl";

// ----------------------------------------------------------------------------
// Ray-Sphere Intersection
// ----------------------------------------------------------------------------
bool intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius, out float t,
                     out vec3 normal, out float discriminant)
{
	vec3 oc = ro - center;
	float b = dot(oc, rd);
	float c = dot(oc, oc) - radius * radius;
	float h = b * b - c;

	discriminant = h;

	if (h < 0.0)
		return false;  // No intersection

	h = sqrt(h);
	// Intersection t
	t = -b - h;

	if (t < 0.0) {
		return false;
	}

	vec3 hitPos = ro + t * rd;
	normal = normalize(hitPos - center);
	return true;
}

void main()
{
	// 1. Calculate Ray direction
	// In Ortho/Persp generic: normalize(WorldPos - camPos).
	// Note: WorldPos comes from VS, it's on the billboard plane.
	vec3 rayDir = normalize(WorldPos - camPos);
	vec3 rayOrigin = camPos;

	float t;
	vec3 N;
	float h;  // Discriminant
	bool hit = intersectSphere(rayOrigin, rayDir, SphereCenter,
	                           SphereRadius, t, N, h);

	if (!hit) {
		discard;
	}

	// Analytic Edge Smoothing (Pseudo-AA)
	float edgeFactor = clamp(h / fwidth(h), 0.0, 1.0);
	edgeFactor = smoothstep(0.0, 1.0, edgeFactor);

	vec3 sphereHitPos = rayOrigin + t * rayDir;

	// 2. Correct Depth
	vec4 clipPos = projection * view * vec4(sphereHitPos, 1.0);
	float ndcDepth = clipPos.z / clipPos.w;
	gl_FragDepth = (gl_DepthRange.diff * ndcDepth + gl_DepthRange.near +
	                gl_DepthRange.far) *
	               0.5;

	// 3. Lighting
	vec3 V = -rayDir;  // View vector is towards camera

	vec3 color;
	if (debugMode != 0) {
		color = compute_debug(N, V, Albedo, Metallic, Roughness, AO,
		                      debugMode);
	} else {
		color = compute_pbr(N, V, Albedo, Metallic, Roughness, AO);
	}

	// Apply Edge Smoothing (Darken rim)
	// This simulates coverage by fading to black/background.
	// Since we are opaque, we can't blend with BG, but darkening helps
	// reduce the harsh staircase effect.
	color *= edgeFactor;

#ifdef USE_TRANSPARENT_BILLBOARDS
	// Transparent Mode: Alpha = Opacity (edgeFactor) for Blending
	FragColor = vec4(color, edgeFactor);
#else
	// Legacy Opaque Mode: Alpha = Luma for FXAA Optimization
	float luma = dot(sqrt(color), vec3(0.299, 0.587, 0.114));
	FragColor = vec4(color, luma);
#endif

	// --- Velocity Calculation ---
	// We assume the object is static, so WorldPos is the same for previous
	// frame. Velocity is purely due to camera movement.

	// Current Clip Position
	vec4 currentClip = projection * view * vec4(sphereHitPos, 1.0);

	// Previous Clip Position
	vec4 previousClip = previousViewProj * vec4(sphereHitPos, 1.0);

	vec2 currentPosNDC = currentClip.xy / currentClip.w;
	vec2 previousPosNDC = previousClip.xy / previousClip.w;

	// UV space velocity (NDC -> UV is * 0.5 + 0.5) implies factor 0.5
	VelocityOut = (currentPosNDC - previousPosNDC) * 0.5;
}
