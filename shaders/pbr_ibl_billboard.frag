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
bool intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius, out float t,
                     out vec3 normal)
{
	vec3 oc = ro - center;
	float b = dot(oc, rd);
	float c = dot(oc, oc) - radius * radius;
	float h = b * b - c;

	bool isHit = true;
	if (h < 0.0) {
		h = 0.0;
		isHit = false;
	}

	h = sqrt(h);
	// Intersection t
	t = -b - h;

	if (t < 0.0) {
		isHit = false;
	}

	vec3 hitPos = ro + t * rd;
	normal = normalize(hitPos - center);
	return isHit;
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
	bool hit = intersectSphere(rayOrigin, rayDir, SphereCenter,
	                           SphereRadius, t, N);

	vec3 sphereHitPos = rayOrigin + t * rayDir;

	// 2. Correct Depth
	vec4 clipPos = projection * view * vec4(sphereHitPos, 1.0);
	float ndcDepth = clipPos.z / clipPos.w;
	gl_FragDepth = (gl_DepthRange.diff * ndcDepth + gl_DepthRange.near +
	                gl_DepthRange.far) *
	               0.5;

	// 3. Lighting
	vec3 V = -rayDir;  // View vector is towards camera

	// Analytic AA: Boost roughness at grazing angles to prevent
	// fireflies/aliasing The sphere edge produces infinite frequency
	// changes in R. We force the edge to be diffuse-like.
	float NdotV = max(dot(N, V), 0.0);
	float rimRoughness = smoothstep(0.1, 0.01, NdotV);  // Very tight: 0.1
	float adjustedRoughness = max(Roughness, rimRoughness);

	vec3 color;
	if (debugMode != 0) {
		color = compute_debug(N, V, Albedo, Metallic, adjustedRoughness,
		                      AO, debugMode);
	} else {
		// Manual PBR setup to avoid compute_roughness_clamping() which
		// uses dFdx(N). dFdx(N) is unstable at the sphere adge. We rely
		// on Analytic AA (rimRoughness) instead. reflect(I, N) expects
		// Incident vector I (Camera -> Surface), which is rayDir.
		vec3 R = reflect(rayDir, N);
		float clampNdotV = max(dot(N, V), 1e-4);
		vec3 F0 = mix(vec3(0.04), Albedo, Metallic);

		// Geometric AA: Re-introduce roughness clamping based on normal
		// variance This ensures that small/distant spheres look correct
		// (not too shiny).
		vec3 dNdx = dFdx(N);
		vec3 dNdy = dFdy(N);
		float maxVariation = max(dot(dNdx, dNdx), dot(dNdy, dNdy));

		// SAFETY CLAMP: The analytic edge has infinite variation.
		// Mask out the derivative-based AA at the grazing angle where
		// it is unstable and let the Analytic AA take over.

		// 1. Clamp FIRST to handle the singularity (infinity).
		maxVariation = min(maxVariation, 1.0);

		// 2. Strict Mask: Fade out the (now bounded) variation near the
		// edge. TUNED ZONE: Extremely tight. Instability only tolerated
		// in last 2%
		float edgeMask = smoothstep(0.02, 0.1, clampNdotV);
		maxVariation *= edgeMask;
		float geometricRoughness = max(
		    Roughness,
		    pow(maxVariation, 0.1));  // 0.1 matches pbr_functions.glsl

		// Note: adjustedRoughness already includes the rim boost
		// (Analytic AA).
		float finalRoughness =
		    max(geometricRoughness, adjustedRoughness);

		// Final safety floor
		finalRoughness = max(finalRoughness, 0.04);

		// 3. Specular Damping: Fade out F0/Specular at the very edge
		// to prevent potential "sparkles" from unstable normals.
		// Consistent with rimRoughness
		float specularDamping = smoothstep(0.0, 0.05, clampNdotV);
		vec3 dampedF0 = F0 * specularDamping;

		color = compute_IBL_PBR_Advanced(N, V, R, dampedF0, clampNdotV,
		                                 Albedo, Metallic,
		                                 finalRoughness, AO);
	}

	if (!hit) {
		discard;
	}

	FragColor = vec4(color, 1.0);

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
