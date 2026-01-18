#version 440 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;

/* Flags d'activation des effets */
uniform int enableVignette;
uniform int enableGrain;
uniform int enableExposure;
uniform int enableChromAbbr;

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
    /* Reinhard tone mapping avec exposition */
    vec3 mapped = vec3(1.0) - exp(-color * exposure);
    return mapped;
}

/* Effet Aberration Chromatique */
vec3 applyChromAbbr(vec2 uv) {
    vec2 direction = uv - vec2(0.5);
    
    float r = texture(screenTexture, uv + direction * chromAbbrStrength).r;
    float g = texture(screenTexture, uv).g;
    float b = texture(screenTexture, uv - direction * chromAbbrStrength).b;
    
    return vec3(r, g, b);
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
    
    /* Appliquer le vignettage */
    if (enableVignette != 0) {
        color = applyVignette(color, TexCoords);
    }
    
    /* Appliquer le grain (en dernier pour plus de réalisme) */
    if (enableGrain != 0) {
        color = applyGrain(color, TexCoords);
    }
    
    FragColor = vec4(color, 1.0);
}