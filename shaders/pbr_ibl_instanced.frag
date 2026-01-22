#version 450 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec2 VelocityOut;

in vec3 WorldPos;
in vec3 Normal;
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

const float PI = 3.14159265359;
const float INV_PI = 0.31830988618; // 1/PI précalculé
const float EPSILON = 1e-6;

// ----------------------------------------------------------------------------
// Helper: Direction vers UV Equirectangulaire
// ----------------------------------------------------------------------------
vec2 dirToUV(vec3 v) {
    float phi = (abs(v.z) < 1e-5 && abs(v.x) < 1e-5) ? 0.0 : atan(v.z, v.x);
    vec2 uv = vec2(phi, asin(clamp(v.y, -1.0, 1.0)));
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
    // --- DIFFUSE IBL ---
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - Metallic);
    vec3 irradiance = texture(irradianceMap, dirToUV(N)).rgb;
    irradiance = max(irradiance, vec3(0.0)); // Prevent negative light
    vec3 diffuse = irradiance * Albedo;

    // --- SPECULAR IBL (Split-Sum) ---
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, dirToUV(R), roughness * MAX_REFLECTION_LOD).rgb;
    prefilteredColor = max(prefilteredColor, vec3(0.0)); // Prevent negative light

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

    // 1. Calcul de la rugosité anti-scintillement et sécurisée
    float clamped_roughness = compute_roughness_clamping(N, Roughness);
    clamped_roughness = max(clamped_roughness, 0.04); /* Prevent 0.0 roughness singularity */

    // 2. Appel à la fonction complète avec compensation d'énergie
    vec3 ambient = compute_IBL_PBR_Advanced(N, V, R, F0, NdotV, clamped_roughness);

    vec3 color = ambient;

    // --- DEBUG VIEW ---
    // 0: Final PBR
    // 1: Albedo
    // 2: Normal
    // 3: Metallic
    // 4: Roughness
    // 5: AO
    // 6: Irradiance (Diffuse)
    // 7: Prefilter (Specular)
    // 8: BRDF LUT

    if (debugMode != 0) {
        if (debugMode == 1) color = vec3(Albedo);
        else if (debugMode == 2) color = N * 0.5 + 0.5;
        else if (debugMode == 3) color = vec3(Metallic);
        else if (debugMode == 4) color = vec3(Roughness);
        else if (debugMode == 5) color = vec3(AO);
        else if (debugMode == 6) {
             vec3 irradiance = texture(irradianceMap, dirToUV(N)).rgb;
             /* Gamma correct for visualization */
             color = pow(irradiance, vec3(1.0/2.2));
        }
        else if (debugMode == 7) {
             const float MAX_REFLECTION_LOD = 4.0;
             vec3 prefiltered = textureLod(prefilterMap, dirToUV(R), Roughness * MAX_REFLECTION_LOD).rgb;
             color = pow(prefiltered, vec3(1.0/2.2));
        }
        else if (debugMode == 8) {
             vec2 brdfUV = vec2(max(dot(N, V), 0.0), Roughness);
             vec2 texSize = vec2(textureSize(brdfLUT, 0));
             brdfUV = brdfUV * (texSize - 1.0) / texSize + 0.5 / texSize;
             color = vec3(texture(brdfLUT, brdfUV).rg, 0.0);
        }
    }

    FragColor = vec4(color, 1.0);

    // Calculate Velocity
    vec2 currentPosNDC = CurrentClipPos.xy / CurrentClipPos.w;
    vec2 previousPosNDC = PreviousClipPos.xy / PreviousClipPos.w;

    // UV space velocity (NDC -> UV is * 0.5 + 0.5) implies factor 0.5
    vec2 velocity = (currentPosNDC - previousPosNDC) * 0.5;
    VelocityOut = velocity;
}
