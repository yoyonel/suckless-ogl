#version 450 core

out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec3 v_Albedo;
in float v_Metallic;
in float v_Roughness;
in float v_AO;

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
// Reproduit exactement la logique de ton shader Python
// ----------------------------------------------------------------------------
vec3 compute_IBL_PBR(vec3 N, vec3 V, vec3 R, vec3 F0, float NdotV)
{
    // Fresnel pour IBL
    vec3 F = fresnelSchlickRoughness(NdotV, F0, v_Roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - v_Metallic);

    // --- DIFFUSE IBL ---
    vec3 irradiance = texture(irradianceMap, dirToUV(N)).rgb;
    vec3 diffuse = irradiance * v_Albedo;

    // --- SPECULAR IBL ---
    // Calcul dynamique du nombre de LODs (comme dans ton shader Python)
    // Note: en GLSL, textureQueryLevels ne fonctionne qu'avec samplerCube
    // Pour sampler2D, on utilise une valeur fixe ou un uniform
    const float MAX_REFLECTION_LOD = 4.0; // Ajuste selon tes mipmaps
    
    vec3 prefilteredColor = textureLod(prefilterMap, dirToUV(R), v_Roughness * MAX_REFLECTION_LOD).rgb;

    // BRDF LUT lookup avec correction de coordonnées pour éviter artifacts aux bords
    vec2 brdfUV = vec2(NdotV, v_Roughness);
    
    // Cette correction évite les artifacts aux bords de la texture
    vec2 texSize = vec2(textureSize(brdfLUT, 0));
    brdfUV = brdfUV * (texSize - 1.0) / texSize + 0.5 / texSize;
    
    vec2 brdf = texture(brdfLUT, brdfUV).rg;

    // --- MULTIPLE SCATTERING COMPENSATION ---
    // Split-sum approximation avec compensation de multiple scattering
    vec3 FssEss = F * brdf.x + brdf.y;

    // Average Fresnel pour multiple scattering
    // Cette approximation capture la réflectance moyenne sur toutes les directions
    vec3 Favg = F0 + (1.0 - F0) * (1.0 / 21.0);
    float Ess = brdf.x + brdf.y;
    
    // Terme de multiple scattering
    vec3 Fms = Favg * FssEss / max(1.0 - Favg * (1.0 - Ess), EPSILON);
    vec3 multipleScattering = Fms * (1.0 - Ess);

    vec3 specular = prefilteredColor * (FssEss + multipleScattering);

    // Energy conservation finale avec multiple scattering
    kD = (1.0 - (FssEss + multipleScattering)) * (1.0 - v_Metallic);

    // Combinaison finale avec AO
    vec3 ambient = (kD * diffuse + specular) * v_AO;
    
    return ambient;
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

void main() {
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);
    vec3 R = reflect(-V, N);

    float NdotV = max(dot(N, V), 0.0);
    vec3 F0 = mix(vec3(0.04), v_Albedo, v_Metallic);

    // IBL Reflection
    vec3 F = fresnelSchlickRoughness(NdotV, F0, v_Roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - v_Metallic);

    // Diffuse (Irradiance)
    vec3 irradiance = texture(irradianceMap, dirToUV(N)).rgb;
    vec3 diffuse = irradiance * v_Albedo;

    // Specular (Prefiltered)
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, dirToUV(R), v_Roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF = texture(brdfLUT, vec2(NdotV, v_Roughness)).rg;
    vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    vec3 ambient = (kD * diffuse + specular) * v_AO;

    vec3 color = ambient * pbr_exposure;
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}