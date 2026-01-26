// ----------------------------------------------------------------------------
// Common PBR & IBL Functions
// ----------------------------------------------------------------------------

const float PI = 3.14159265359;
const float INV_PI = 0.31830988618;
const float EPSILON = 1e-6;

// ----------------------------------------------------------------------------
// Helper: Direction to Equirectangular UV
// ----------------------------------------------------------------------------
vec2 dirToUV(vec3 v)
{
	float phi = (abs(v.z) < 1e-5 && abs(v.x) < 1e-5) ? 0.0 : atan(v.z, v.x);
	vec2 uv = vec2(phi, asin(clamp(v.y, -1.0, 1.0)));
	uv *= vec2(0.1591, 0.3183);  // 1/2PI, 1/PI
	uv += 0.5;
	uv.y = 1.0 - uv.y;
	return uv;
}

// ----------------------------------------------------------------------------
// Fresnel Schlick with roughness
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	float f = pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * f;
}

// ----------------------------------------------------------------------------
// Main IBL PBR Computation
// Assumes standard uniform names: irradianceMap, prefilterMap, brdfLUT
// ----------------------------------------------------------------------------
vec3 compute_IBL_PBR_Advanced(vec3 N, vec3 V, vec3 R, vec3 F0, float NdotV,
                              vec3 albedo, float metallic, float roughness,
                              float ao)
{
	vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);

	// --- DIFFUSE IBL ---
	vec3 kS = F;
	vec3 kD = (1.0 - kS) * (1.0 - metallic);
	vec3 irradiance = texture(irradianceMap, dirToUV(N)).rgb;
	irradiance = max(irradiance, vec3(0.0));
	vec3 diffuse = irradiance * albedo;

	// --- SPECULAR IBL (Split-Sum) ---
	const float MAX_REFLECTION_LOD = 4.0;
	vec3 prefilteredColor =
	    textureLod(prefilterMap, dirToUV(R), roughness * MAX_REFLECTION_LOD)
	        .rgb;
	prefilteredColor = max(prefilteredColor, vec3(0.0));

	// BRDF LUT lookup
	vec2 brdfUV = vec2(NdotV, roughness);
	vec2 texSize = vec2(textureSize(brdfLUT, 0));
	brdfUV = brdfUV * (texSize - 1.0) / texSize + 0.5 / texSize;
	vec2 brdf = texture(brdfLUT, brdfUV).rg;

	// --- COMPENSATION MULTIPLE SCATTERING ---
	vec3 FssEss = F * brdf.x + brdf.y;
	vec3 Favg = F0 + (1.0 - F0) * (1.0 / 21.0);
	float Ess = brdf.x + brdf.y;
	vec3 Fms = Favg * FssEss / max(1.0 - Favg * (1.0 - Ess), EPSILON);
	vec3 multipleScattering = Fms * (1.0 - Ess);

	vec3 specular = prefilteredColor * (FssEss + multipleScattering);

	// Final Energy Conservation
	kD = (1.0 - (FssEss + multipleScattering)) * (1.0 - metallic);

	return (kD * diffuse + specular) * ao;
}

// ----------------------------------------------------------------------------
// Roughness Clamping (Anti-Aliasing)
// ----------------------------------------------------------------------------
float compute_roughness_clamping(vec3 N, float roughness)
{
	vec3 dNdx = dFdx(N);
	vec3 dNdy = dFdy(N);
	float maxVariation = max(dot(dNdx, dNdx), dot(dNdy, dNdy));
	float normalThreshold = 0.1;
	roughness = max(roughness, pow(maxVariation, normalThreshold));
	roughness = clamp(roughness, 0.0, 1.0);
	return roughness;
}

// ----------------------------------------------------------------------------
// Master Function: Compute Shading
// ----------------------------------------------------------------------------
vec3 compute_pbr(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
                 float ao)
{
	vec3 R = reflect(-V, N);
	float NdotV = max(dot(N, V), 1e-4);
	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	// 1. Roughness clamping
	float clamped_roughness = compute_roughness_clamping(N, roughness);
	clamped_roughness = max(clamped_roughness, 0.04);

	// 2. Compute PBR
	vec3 color = compute_IBL_PBR_Advanced(N, V, R, F0, NdotV, albedo,
	                                      metallic, clamped_roughness, ao);
	return color;
}

vec3 compute_debug(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
                   float ao, int debugMode)
{
	vec3 color = vec3(0.0);
	// 3. Debug Overrides
	if (debugMode != 0) {
		if (debugMode == 1)
			color = vec3(albedo);
		else if (debugMode == 2)
			color = N * 0.5 + 0.5;
		else if (debugMode == 3)
			color = vec3(metallic);
		else if (debugMode == 4)
			color = vec3(roughness);
		else if (debugMode == 5)
			color = vec3(ao);
		else if (debugMode == 6) {
			vec3 irradiance =
			    texture(irradianceMap, dirToUV(N)).rgb;
			color = pow(irradiance, vec3(1.0 / 2.2));
		} else if (debugMode == 7) {
			vec3 R = reflect(-V, N);
			const float MAX_REFLECTION_LOD = 4.0;
			vec3 prefiltered =
			    textureLod(prefilterMap, dirToUV(R),
			               roughness * MAX_REFLECTION_LOD)
			        .rgb;
			color = pow(prefiltered, vec3(1.0 / 2.2));
		} else if (debugMode == 8) {
			float NdotV = max(dot(N, V), 0.0);
			vec2 brdfUV = vec2(NdotV, roughness);
			vec2 texSize = vec2(textureSize(brdfLUT, 0));
			brdfUV =
			    brdfUV * (texSize - 1.0) / texSize + 0.5 / texSize;
			color = vec3(texture(brdfLUT, brdfUV).rg, 0.0);
		}
	}
	return color;
}
