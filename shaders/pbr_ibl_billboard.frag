#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 VelocityOut;

in vec3 WorldPos;  // Position on the billboard plane
in vec3 SphereCenter;
in float SphereRadius;
in vec3 Albedo;
in float Metallic;
in float Roughness;
in float AO;

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
void main()
{
	// 1. Calculate Ray direction
	vec3 rayDir = normalize(WorldPos - camPos);
	vec3 rayOrigin = camPos;

	// Ray-Sphere Intersection (Analytic w/ Alpha-to-Coverage)
	vec3 oc = rayOrigin - SphereCenter;
	float b = dot(oc, rayDir);
	float c = dot(oc, oc) - SphereRadius * SphereRadius;
	float h = b * b - c;

	// Alpha-to-Coverage: Compute alpha based on signed distance
	// discriminant h < 0 (miss), h > 0 (hit). Transition happens over
	// fwidth(h). We use 0.5 offset so h=0 implies 50% coverage.
	float alpha = clamp(0.5 + h / fwidth(h), 0.0, 1.0);

	// Note: We DO NOT discard here.
	// Discarding helper pixels destroys dFdx(N) derivatives for the edge
	// pixels, causing black artifacts. We let the shader run and let
	// A2C/Alpha handle visibility.

	// SAFETY: We MUST discard pixels that are completely transparent
	// (alpha=0) otherwise we see the quad corners if A2C is not perfect or
	// blending is off. The derivative artifacts (black pixels) at the edge
	// boundary are hidden by the 'edgeMask' later in the shader.
	if (alpha <= 0.0) {
		discard;
	}

	// For shading, clamp h to valid range (inside sphere)
	// identifying the "closest point" on the silhouette for the edge
	// pixels.
	float effectiveH = max(h, 0.0);
	float t = -b - sqrt(effectiveH);

	// Calculate Position & Normal
	vec3 sphereHitPos = rayOrigin + t * rayDir;
	vec3 N = normalize(sphereHitPos - SphereCenter);

	// 2. Correct Depth
	vec4 clipPos = projection * view * vec4(sphereHitPos, 1.0);
	float ndcDepth = clipPos.z / clipPos.w;
	gl_FragDepth = (gl_DepthRange.diff * ndcDepth + gl_DepthRange.near +
	                gl_DepthRange.far) *
	               0.5;

	// 3. Lighting
	vec3 V = -rayDir;  // View vector is towards camera

	// Analytic AA: Boost roughness at grazing angles
	// A2C handles geometric aliasing, so we only need a very subtle
	// roughness boost to prevent specular aliasing (fireflies) inside the
	// mask.
	float NdotV = max(dot(N, V), 0.0);
	float rimRoughness = smoothstep(0.04, 0.005, NdotV);  // Relaxed for A2C
	float adjustedRoughness = max(Roughness, rimRoughness);

	vec3 color;
	if (debugMode != 0) {
		color = compute_debug(N, V, Albedo, Metallic, adjustedRoughness,
		                      AO, debugMode);
	} else {
		vec3 R = reflect(rayDir, N);
		float clampNdotV = max(dot(N, V), 1e-4);
		vec3 F0 = mix(vec3(0.04), Albedo, Metallic);

		// Geometric AA
		vec3 dNdx = dFdx(N);
		vec3 dNdy = dFdy(N);
		float maxVariation = max(dot(dNdx, dNdx), dot(dNdy, dNdy));
		maxVariation = min(maxVariation, 1.0);

		float edgeMask = smoothstep(0.02, 0.1, clampNdotV);
		maxVariation *= edgeMask;
		float geometricRoughness =
		    max(Roughness, pow(maxVariation, 0.1));

		float finalRoughness =
		    max(geometricRoughness, adjustedRoughness);
		finalRoughness = max(finalRoughness, 0.04);

		// Removed specularDamping: It was causing black halos on
		// metallic objects by killing the specular contribution (the
		// only light source) at the edge. A2C handles the geometric
		// aliasing now.

		color =
		    compute_IBL_PBR_Advanced(N, V, R, F0, clampNdotV, Albedo,
		                             Metallic, finalRoughness, AO);
	}

	// Output Color with Weighted Alpha for A2C
	FragColor = vec4(color, alpha);

	// --- Velocity Calculation ---
	vec4 currentClip = projection * view * vec4(sphereHitPos, 1.0);
	vec4 previousClip = previousViewProj * vec4(sphereHitPos, 1.0);
	vec2 currentPosNDC = currentClip.xy / currentClip.w;
	vec2 previousPosNDC = previousClip.xy / previousClip.w;
	VelocityOut = (currentPosNDC - previousPosNDC) * 0.5;
}
