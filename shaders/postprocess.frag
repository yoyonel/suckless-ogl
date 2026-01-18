#version 440 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;

/* Flags d'activation des effets */
uniform int enableVignette;
uniform int enableGrain;
uniform int enableExposure;
uniform int enableChromAbbr;
uniform int enableColorGrading;

/* Paramètres Vignette */
uniform float vignetteIntensity;
uniform float vignetteExtent;

/* Paramètres Grain */
uniform float grainIntensity;
uniform float time;

/* Paramètres Exposition */
uniform float exposure;

/* Paramètres Aberration Chromatique */
uniform float chromAbbrStrength;

/* Paramètres Color Grading (Unreal Style) */
uniform float gradSaturation;
uniform float gradContrast;
uniform float gradGamma;
uniform float gradGain;
uniform float gradOffset;

/* Fonction de bruit pseudo-aléatoire pour le grain */
float random(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

/* Effet Vignette */
vec3 applyVignette(vec3 color, vec2 uv) {
    vec2 centered = uv * 2.0 - 1.0;
    float dist = length(centered);
    float vignette = smoothstep(vignetteExtent, vignetteExtent - 0.4, dist);
    return color * mix(1.0, vignette, vignetteIntensity);
}

/* Effet Grain */
vec3 applyGrain(vec3 color, vec2 uv) {
    float noise = random(uv + time) * 2.0 - 1.0;
    return color + noise * grainIntensity;
}

/* Effet Exposition (Tone Mapping) */
vec3 applyExposure(vec3 color) {
    /* Exposition linéaire simple */
    return color * exposure;
}

/* Effet Aberration Chromatique */
vec3 applyChromAbbr(vec2 uv) {
    vec2 direction = uv - vec2(0.5);
    
    float r = texture(screenTexture, uv + direction * chromAbbrStrength).r;
    float g = texture(screenTexture, uv).g;
    float b = texture(screenTexture, uv - direction * chromAbbrStrength).b;
    
    return vec3(r, g, b);
}

/*
 * ACES Tone Mapping (Approximation Narkowicz)
 * Standard de facto pour le rendu filmique en temps réel (utilisé dans UE4/UE5).
 * Mappe le HDR [0, inf] vers une courbe sigmoïde plaisante en [0, 1].
 */
vec3 aces_film(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

/*
 * Logique Color Grading Unreal Engine
 * Ordre: Saturation -> Contrast -> Gamma -> Gain -> Offset
 * Tout se passe en espace linéaire avant le tone mapping.
 */
vec3 apply_color_grading(vec3 color) {
    /* 1. Saturation */
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, gradSaturation);

    /* 2. Contraste */
    /* Le pivot standard est 0.5 (gris moyen) en espace log, 
       mais en linéaire on utilise souvent ACES pour ça. 
       Ici une approche simple autour du gris moyen linéaire. */
    color = (color - 0.5) * gradContrast + 0.5;
    color = max(vec3(0.0), color); /* Éviter les négatifs */

    /* 3. Gamma */
    if (gradGamma > 0.0) {
        color = pow(color, vec3(gradGamma));
    }

    /* 4. Gain */
    color = color * gradGain;

    /* 5. Offset */
    color = color + gradOffset;
    
    /* Clamp final pour sécurité avant tone map */
    return max(vec3(0.0), color);
}

void main() {
    vec3 color;
    
    /* Si aberration chromatique active, on récupère la couleur différemment */
    if (enableChromAbbr != 0) {
        color = applyChromAbbr(TexCoords);
    } else {
        color = texture(screenTexture, TexCoords).rgb;
    }
    
    /* Appliquer l'exposition en premier (pour le HDR -> LDR) */
    if (enableExposure != 0) {
        color = applyExposure(color);
    }
    
    /* Appliquer le color grading */
    if (enableColorGrading != 0) {
        color = apply_color_grading(color);
    }
    
    /* Tone Mapping (HDR -> LDR Linear) */
    color = aces_film(color);

    /* Appliquer le vignettage */
    if (enableVignette != 0) {
        color = applyVignette(color, TexCoords);
    }
    
    /* Correction Gamma (Linear -> sRGB) */
    color = pow(color, vec3(1.0 / 2.2));

    /* Appliquer le grain (en dernier pour plus de réalisme) */
    if (enableGrain != 0) {
        color = applyGrain(color, TexCoords);
    }
    
    FragColor = vec4(color, 1.0);
}