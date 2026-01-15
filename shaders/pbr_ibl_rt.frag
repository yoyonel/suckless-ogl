#version 450 core

in vec2 LocalPos;
in float SphereRadius;
in vec3 CenterVS;

out vec4 FragColor;

uniform mat4 projection;
uniform mat4 invView;

struct Material {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
};
uniform Material material;

uniform float pbr_exposure;
uniform float aa_width_multiplier;
uniform float aa_distance_factor;

// IBL Textures (Sampler2D car on utilise l'equirectangulaire)
uniform sampler2D irradianceMap;
uniform sampler2D prefilterMap;
uniform sampler2D brdfLUT;

uniform vec3 camPos;

const float PI = 3.14159265359;
const float INV_PI = 0.31830988618; // 1/PI précalculé
const float MIN_ROUGHNESS = 0.02;   // Roughness minimum pour stabilité
const float EPSILON = 1e-6;         // Pour éviter divisions par zéro

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
    vec3 F = fresnelSchlickRoughness(NdotV, F0, material.roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - material.metallic);

    // --- DIFFUSE IBL ---
    vec3 irradiance = texture(irradianceMap, dirToUV(N)).rgb;
    vec3 diffuse = irradiance * material.albedo;

    // --- SPECULAR IBL ---
    // Calcul dynamique du nombre de LODs (comme dans ton shader Python)
    // Note: en GLSL, textureQueryLevels ne fonctionne qu'avec samplerCube
    // Pour sampler2D, on utilise une valeur fixe ou un uniform
    const float MAX_REFLECTION_LOD = 4.0; // Ajuste selon tes mipmaps
    
    vec3 prefilteredColor = textureLod(prefilterMap, dirToUV(R), material.roughness * MAX_REFLECTION_LOD).rgb;

    // BRDF LUT lookup avec correction de coordonnées pour éviter artifacts aux bords
    vec2 brdfUV = vec2(NdotV, material.roughness);
    
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
    kD = (1.0 - (FssEss + multipleScattering)) * (1.0 - material.metallic);

    // Combinaison finale avec AO
    vec3 ambient = (kD * diffuse + specular) * material.ao;
    
    return vec3(dirToUV(R), 0);
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

// ----------------------------------------------------------------------------
// Raytrace sphere pour billboarding
// Modifié pour supporter l'AA analytique (retourne 'true' pour les frôlements)
// --- DANS pbr_ibl_rt.frag ---

bool raytrace_sphere(out vec3 N, out vec3 V, out vec3 fragWorldPos) {
    vec3 O = vec3(0.0); 
    vec3 P = CenterVS + vec3(LocalPos, 0.0); 
    vec3 D = normalize(P - O);

    float b = -2.0 * dot(D, CenterVS);
    float c = dot(CenterVS, CenterVS) - (SphereRadius * SphereRadius);
    float delta = b * b - 4.0 * c;

    if (delta < 0.0) return false;

    float t = (-b - sqrt(delta)) * 0.5;
    if (t < 0.0) return false;

    // Position du point d'impact en View Space
    vec3 hit_vs = O + t * D;

    // --- SYNCHRONISATION ESPACE MONDE ---
    
    // 1. Position Monde (ISO avec WorldPos de l'icosphère)
    vec4 hit_world = invView * vec4(hit_vs, 1.0);
    fragWorldPos = hit_world.xyz;

    // 2. Normale Monde (ISO avec Normal de l'icosphère)
    // On calcule la normale en VS, puis on applique SEULEMENT la rotation de l'invView
    vec3 normal_vs = (hit_vs - CenterVS) / SphereRadius;
    N = normalize(mat3(invView) * normal_vs);

    // 3. Vecteur Vue Monde (ISO avec V de l'icosphère)
    V = normalize(camPos - fragWorldPos);

    // 4. Correction de profondeur (pour que le raytrace respecte le Z-buffer)
    vec4 clip_pos = projection * vec4(hit_vs, 1.0);
    gl_FragDepth = (clip_pos.z / clip_pos.w) * 0.5 + 0.5;

    return true;
}

// ----------------------------------------------------------------------------
void main()
{
    vec3 N;
    vec3 V;
    vec3 fragWorldPos;

    // Alpha coverage for analytic AA (default opaque)
    float coverage = 1.0;

    // ------------------------------------------------------------------------
    // Geometry evaluation
    // ------------------------------------------------------------------------
    // Analytic ray–sphere intersection
    if (!raytrace_sphere(N, V, fragWorldPos)) {
        discard;
    }

    // // --------------------------------------------------------------------
    // // Analytic edge anti-aliasing (stable version)
    // //
    // // LocalPos is in [-2, 2] because QUAD_SCALE = 2.0
    // // The projected sphere radius in this space is exactly 2.0
    // // --------------------------------------------------------------------
    // const float radius = 1.0;

    // // Signed distance to sphere edge in billboard plane
    // float edgeDist = radius - length(LocalPos);

    // // Screen-space pixel footprint of the implicit edge
    // // Réglable via aa_width_multiplier et aa_distance_factor
    // float dist = length(CenterVS);
    // float edgeWidth = fwidth(edgeDist) * (aa_width_multiplier + dist * aa_distance_factor);

    // // Prevent sub-pixel instability at far distance
    // edgeWidth = max(edgeWidth, 1.0 / 1024.0);

    // // Smooth analytic coverage
    // coverage = smoothstep(-edgeWidth, edgeWidth, edgeDist);

    // // Hard reject only pixels clearly outside the transition zone
    // if (edgeDist < -edgeWidth) {
    //     discard;
    // }
    
    // ------------------------------------------------------------------------
    // Common PBR inputs
    // ------------------------------------------------------------------------
    vec3 R = reflect(-V, N);
    vec3 F0 = mix(vec3(0.04), material.albedo, material.metallic);
    float NdotV = max(dot(N, V), 0.0);

    // ------------------------------------------------------------------------
    // Image-Based Lighting (IBL)
    // ------------------------------------------------------------------------
    vec3 ambient = compute_IBL_PBR(N, V, R, F0, NdotV);

    // ------------------------------------------------------------------------
    // Final shading
    // ------------------------------------------------------------------------
    vec3 color = ambient;
    color *= pbr_exposure;

    // ------------------------------------------------------------------------
    // Tonemapping & gamma correction
    // ------------------------------------------------------------------------
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(ambient, 1.0);
}