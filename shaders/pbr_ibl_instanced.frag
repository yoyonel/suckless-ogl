#version 450 core

out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec3 Albedo;
in float Metallic;
in float Roughness;
in float AO;

uniform vec3 camPos;
uniform sampler2D irradianceMap;
uniform sampler2D prefilterMap;
uniform sampler2D brdfLUT;
uniform float pbr_exposure;

const float PI = 3.14159265359;
const float INV_PI = 0.31830988618; // 1/PI précalculé
const float EPSILON = 1e-6;

// ----------------------------------------------------------------------------
// Helper: Direction vers UV Equirectangulaire
// ----------------------------------------------------------------------------
vec2 dirToUV(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(0.1591, 0.3183); // 1/2PI, 1/PI
    uv += 0.5;
    
    // INVERSION VERTICALE
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
// compute_IBL_PBR avec Multiple Scattering
// ----------------------------------------------------------------------------
vec3 compute_IBL_PBR_Advanced(vec3 N, vec3 V, vec3 R, vec3 F0, float NdotV, float roughness)
{
    // Fresnel avec rugosité corrigée
    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    
    // --- DIFFUSE IBL ---
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - Metallic);
    vec3 irradiance = texture(irradianceMap, dirToUV(N)).rgb;
    vec3 diffuse = irradiance * Albedo;

    // --- SPECULAR IBL (Split-Sum) ---
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, dirToUV(R), roughness * MAX_REFLECTION_LOD).rgb;
    
    // BRDF LUT lookup
    vec2 brdfUV = vec2(NdotV, roughness);
    vec2 texSize = vec2(textureSize(brdfLUT, 0));
    brdfUV = brdfUV * (texSize - 1.0) / texSize + 0.5 / texSize; // Correction de bord
    vec2 brdf = texture(brdfLUT, brdfUV).rg;
    
    // --- COMPENSATION MULTIPLE SCATTERING ---
    vec3 FssEss = F * brdf.x + brdf.y;
    vec3 Favg = F0 + (1.0 - F0) * (1.0 / 21.0);
    float Ess = brdf.x + brdf.y;
    vec3 Fms = Favg * FssEss / max(1.0 - Favg * (1.0 - Ess), EPSILON);
    vec3 multipleScattering = Fms * (1.0 - Ess);

    vec3 specular = prefilteredColor * (FssEss + multipleScattering);
    
    // Conservation d'énergie finale
    kD = (1.0 - (FssEss + multipleScattering)) * (1.0 - Metallic);
    
    return (kD * diffuse + specular) * AO;
}

// ----------------------------------------------------------------------------
// ACES Filmic Tonemapping
// Courbe de tonemapping utilisée dans l'industrie du cinéma
// ----------------------------------------------------------------------------
vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float compute_roughness_clamping(vec3 N, float roughness) {
    // --- Roughness Clamping (Anti-Aliasing Spéculaire) ---
    // On calcule la variation de la normale dans l'espace écran
    vec3 dNdx = dFdx(N);
    vec3 dNdy = dFdy(N);
    float maxVariation = max(dot(dNdx, dNdx), dot(dNdy, dNdy));

    // On ajuste la rugosité minimale : plus la normale change vite, 
    // plus on augmente la rugosité pour "étaler" le reflet et éviter le scintillement.
    float normalThreshold = 0.1; // Ajustable selon le besoin
    roughness = max(roughness, pow(maxVariation, normalThreshold));
    roughness = clamp(roughness, 0.0, 1.0);
    return roughness;
}

void main() {
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);
    vec3 R = reflect(-V, N);

    float NdotV = max(dot(N, V), 0.0);
    vec3 F0 = mix(vec3(0.04), Albedo, Metallic);

    // 1. Calcul de la rugosité anti-scintillement
    float clamped_roughness = compute_roughness_clamping(N, Roughness);

    // 2. Appel à la fonction complète avec compensation d'énergie
    vec3 ambient = compute_IBL_PBR_Advanced(N, V, R, F0, NdotV, clamped_roughness);

    // 3. Post-processing
    vec3 color = ambient * pbr_exposure;
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}