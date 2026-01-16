#version 440 core

out vec4 FragColor;

in vec3 FragPos;
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
const float INV_PI = 0.31830988618;
const float EPSILON = 1e-6;

// ----------------------------------------------------------------------------
// Helper: Direction vers UV Equirectangulaire (IDENTIQUE au shader instancié)
// ----------------------------------------------------------------------------
vec2 dirToUV(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(0.1591, 0.3183); // 1/2PI, 1/PI
    uv += 0.5;
    
    // INVERSION VERTICALE (comme dans le shader instancié)
    uv.y = 1.0 - uv.y; 
    
    return uv;
}

// ----------------------------------------------------------------------------
// Fresnel Schlick with roughness (IDENTIQUE)
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    float f = pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * f;
}

// ----------------------------------------------------------------------------
// ACES Filmic Tonemapping (IDENTIQUE au shader instancié)
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

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - FragPos);
    vec3 R = reflect(-V, N);

    float NdotV = max(dot(N, V), 0.0);
    vec3 F0 = mix(vec3(0.04), Albedo, Metallic);

    // IBL Reflection (IDENTIQUE)
    vec3 F = fresnelSchlickRoughness(NdotV, F0, Roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - Metallic);

    // Diffuse (Irradiance) - Utilisation de dirToUV
    vec3 irradiance = texture(irradianceMap, dirToUV(N)).rgb;
    vec3 diffuse = irradiance * Albedo;

    // Specular (Prefiltered) - Utilisation de dirToUV
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, dirToUV(R), Roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF = texture(brdfLUT, vec2(NdotV, Roughness)).rg;
    vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    // Combinaison finale (IDENTIQUE)
    vec3 ambient = (kD * diffuse + specular) * AO;

    // TONEMAPPING IDENTIQUE au shader instancié
    vec3 color = ambient * pbr_exposure;
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}