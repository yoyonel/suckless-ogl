#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 VelocityOut;

in vec3 WorldPos;  // Position on the billboard plane
in vec3 Normal;    // (Unused)
flat in vec3 SphereCenter;
flat in float SphereRadius;
flat in vec3 Albedo;
flat in float Metallic;
flat in float Roughness;
flat in float AO;

in vec4 CurrentClipPos;  // Interpolated clip pos of the quad (juste pour l'AA)

uniform vec3 camPos;
uniform sampler2D irradianceMap;
uniform sampler2D prefilterMap;
uniform sampler2D brdfLUT;
uniform int debugMode;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 previousViewProj;
uniform vec2 u_screenSize;

// Include common PBR functions
@header "pbr_functions.glsl";

// ----------------------------------------------------------------------------
// Ray-Sphere Intersection
// ----------------------------------------------------------------------------
bool intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius, out float t,
                     out vec3 normal, out float discriminant, out bool isInside)
{
	vec3 oc = ro - center;
	float b = dot(oc, rd);
	float c = dot(oc, oc) - radius * radius;
	float h = b * b - c;

	discriminant = h;
	isInside = (c < 0.0);

	if (h < 0.0)
		return false;

	h = sqrt(h);
	float t1 = -b - h;
	float t2 = -b + h;

	// Pick closest positive hit
	if (t1 >= 0.0)
		t = t1;
	else if (t2 >= 0.0)
		t = t2;
	else
		return false;

	vec3 hitPos = ro + t * rd;
	normal = normalize(hitPos - center);
	return true;
}

void main()
{
	vec3 color;

	// 1. Calculate Ray Geometry
	vec3 rayDir = normalize(WorldPos - camPos);
	vec3 rayOrigin = camPos;

	float t;
	vec3 N;
	float h;
	bool isInside;

	// 2. Raytrace Sphere
	bool hit = intersectSphere(rayOrigin, rayDir, SphereCenter,
	                           SphereRadius, t, N, h, isInside);

	if (!hit) {
		discard;
	}

	// 3. Analytic Edge Smoothing (Anti-Aliasing)
	float pixelSizeWorld =
	    (2.0 * CurrentClipPos.w) / (projection[1][1] * u_screenSize.y);
	float analyticFwidthH = 2.0 * SphereRadius * pixelSizeWorld;
	float edgeFactor = clamp(h / max(analyticFwidthH, 1e-4), 0.0, 1.0);
	edgeFactor = smoothstep(0.0, 1.0, edgeFactor);

	// ------------------------------------------------------------------------
	// RECONSTRUCTION DE LA POSITION EXACTE
	// ------------------------------------------------------------------------
	vec3 sphereHitPos = rayOrigin + t * rayDir;

	// 4. Correct Depth Writing & Current Clip Calculation
	// On projette le point réel de la sphère, pas le plan du billboard
	vec4 clipPosActual = projection * view * vec4(sphereHitPos, 1.0);
	float ndcDepth = clipPosActual.z / clipPosActual.w;

	// Ecriture explicite de la profondeur pour que la sphère soit "ronde"
	// dans le Z-Buffer
	gl_FragDepth = (gl_DepthRange.diff * ndcDepth + gl_DepthRange.near +
	                gl_DepthRange.far) *
	               0.5;

	// 5. Lighting / Shading
	vec3 V = -rayDir;
	if (debugMode != 0) {
		color = compute_debug(N, V, Albedo, Metallic, Roughness, AO,
		                      debugMode, sphereHitPos);
	} else {
		vec3 R_vec = reflect(-V, N);
		float NdotV = max(dot(N, V), 0.0);
		vec3 F0 = mix(vec3(0.04), Albedo, Metallic);
		float analytic_roughness = compute_roughness_clamping_analytic(
		    Roughness, 1.0 / SphereRadius);

		color = compute_IBL_PBR_Advanced(
		    N, V, R_vec, F0, NdotV, Albedo, Metallic,
		    max(analytic_roughness, 0.04), AO, sphereHitPos);
	}

	// Apply Edge AA
	if (!isInside) {
		color *= edgeFactor;
	}

#ifdef USE_TRANSPARENT_BILLBOARDS
	FragColor = vec4(color, edgeFactor);
#else
	float luma = dot(sqrt(color), vec3(0.299, 0.587, 0.114));
	FragColor = vec4(color, luma);
#endif

	// ------------------------------------------------------------------------
	// VELOCITY CALCULATION (OPTIMIZATION "HACK")
	// ------------------------------------------------------------------------
	// Ici, on imite le comportement du vertex shader mesh, mais pixel par
	// pixel.

	// A. Position NDC Actuelle (Basée sur le point raytracé exact)
	vec2 currentPosNDC = clipPosActual.xy / clipPosActual.w;

	// B. Position NDC Précédente
	// HACK : On suppose que la sphère est statique dans le monde (WorldPos
	// frame N == WorldPos frame N-1). On projette sphereHitPos avec la
	// matrice caméra précédente.
	vec4 previousClip = previousViewProj * vec4(sphereHitPos, 1.0);
	vec2 previousPosNDC = previousClip.xy / previousClip.w;

	// C. Calcul du Delta
	// Le facteur 0.5 convertit l'espace NDC [-1,1] vers l'espace UV [0,1]
	VelocityOut = (currentPosNDC - previousPosNDC) * 0.5;
}
