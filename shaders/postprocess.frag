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
uniform float grainIntensityShadows;
uniform float grainIntensityMidtones;
uniform float grainIntensityHighlights;
uniform float grainShadowsMax;
uniform float grainHighlightsMin;
uniform float grainTexelSize;
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

/* Paramètres Bloom */
uniform sampler2D bloomTexture;
/* Depth Texture pour DoF */
uniform sampler2D depthTexture;

uniform int enableBloom;
uniform int enableDoF; /* Flag d'activation DoF */
uniform int enableDoFDebug; /* Flag de debug DoF */
uniform int enableAutoExposure; /* Flag Auto Exposure */
uniform int enableExposureDebug; /* Flag Debug Auto Exposure */

uniform float bloomIntensity;
uniform sampler2D autoExposureTexture; /* Texture 1x1 R32F */

/* Paramètres DoF */
uniform float dofFocalDistance;
uniform float dofFocalRange;
uniform float dofBokehScale;

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
    /* 1. Calculate Luminance */
    float luma = dot(color, vec3(0.299, 0.587, 0.114));

    /* 2. Calculate Intensity based on Luminance Ranges */
    /* Shadows: [0, shadowsMax] */
    float shadowMask = 1.0 - smoothstep(0.0, grainShadowsMax, luma);
    
    /* Highlights: [highlightsMin, 1.0] */
    float highlightMask = smoothstep(grainHighlightsMin, 1.0, luma);
    
    /* Midtones: Fill the gap */
    float midtoneMask = 1.0 - shadowMask - highlightMask;
    midtoneMask = max(0.0, midtoneMask);

    /* Composite Multiplier */
    float lumaMult = shadowMask * grainIntensityShadows +
                     midtoneMask * grainIntensityMidtones +
                     highlightMask * grainIntensityHighlights;

    /* 3. Generate Noise with Texel Size */
    vec2 resolution = vec2(textureSize(screenTexture, 0));
    vec2 coord = uv * resolution / grainTexelSize;
    
    /* Use floor to pixelate the noise (Texel Size effect) */
    vec2 noiseCoord = floor(coord);
    
    /* Add time offset to animate */
    float noise = random(noiseCoord + vec2(time)) * 2.0 - 1.0;

    /* 4. Apply Grain */
    /* Overlay blend mode approximation or simple additive?
       Simple additive is standard for digital grain, but modulation by color 
       can be better. Current was additive: color + noise * intensity. */
    return color + noise * grainIntensity * lumaMult;
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
    
    /* Skybox Hack: Depth ~ 1.0 */
    float depth = texture(depthTexture, TexCoords).r;
    bool isSkybox = depth >= 0.99999;

    /* Si aberration chromatique active et PAS sur la skybox */
    if (enableChromAbbr != 0 && !isSkybox) {
        color = applyChromAbbr(TexCoords);
    } else {
        color = texture(screenTexture, TexCoords).rgb;
    }
    
    
    /* --------------------------------------------------------
       Depth of Field (DoF) - "Cinematic" / Unreal Style Logic
       -------------------------------------------------------- */
    if (enableDoF != 0) {
        float depth = texture(depthTexture, TexCoords).r;
        
        /* Lineariser la depth pour avoir des unités réelles */
        /* Ces valeurs doivent correspondre à celles de l'application (app_settings.h) */
        float zNear = 0.1;
        float zFar = 1000.0; 
        
        /* Formule de linéarisation pour projection perspective standard */
        /* z_ndc = 2.0 * depth - 1.0 */
        /* dist = (2.0 * zNear * zFar) / (zFar + zNear - z_ndc * (zFar - zNear)) */
        float z_ndc = 2.0 * depth - 1.0;
        float dist = (2.0 * zNear * zFar) / (zFar + zNear - z_ndc * (zFar - zNear));
        
        /* 1. Calcul du Circle of Confusion (CoC) */
        /* CoC = |1 - FocalDist / Dist| * Scale */
        float coc = abs(dist - dofFocalDistance) / (dist + 0.0001); /* Eviter div by zero */
        float blurAmount = clamp(coc * dofBokehScale, 0.0, 1.0);
        
        /* Si on est dans le "Focal Range", on reste net */
        if (dist > dofFocalDistance - dofFocalRange && dist < dofFocalDistance + dofFocalRange) {
             blurAmount = 0.0;
        } else {
             /* Transition douce pour éviter le popping */
             float edge = dofFocalRange;
             float distDiff = abs(dist - dofFocalDistance);
             if (distDiff < edge + 5.0) { /* 5.0 unités de transition */
                 blurAmount *= (distDiff - edge) / 5.0;
             }
        }
        
        /* HACK: Skybox Fix. Si depth est très proche de 1.0 (Far Plane), on ne floute pas.
           Avec zFar=1000.0 et D32F, 1.0 est la skybox. On utilise un seuil très strict. */
        if (depth >= 0.99999) {
            blurAmount = 0.0;
        }
        
        blurAmount = clamp(blurAmount, 0.0, 1.0);
        
        /* 2. Bokeh Blur (Poisson Disk Sampling simplifié) */
        /* Pour l'instant, un simple Box Blur pondéré par le CoC pour tester */
        /* Optimisation: Ne faire le blur que si blurAmount > epsilon */
        
        if (blurAmount > 0.01) {
            vec3 acc = vec3(0.0);
            float totalWeight = 0.0;
            
            /* Rayon du disque de flou en pixels (max) */
            float maxRadius = 10.0 * blurAmount; /* Rayon un peu plus large pour compenser le meilleur sampling */
            
            vec2 texSize = vec2(textureSize(screenTexture, 0));
            vec2 pixelSize = 1.0 / texSize;
            
            /* Golden Angle Spiral Sampling for Pattern-less Bokeh */
            /* 16 samples is a good balance for quality vs perf */
            int samples = 16;
            float goldenAngle = 2.39996323;
            
            for(int i = 0; i < samples; i++) {
                 float theta = float(i) * goldenAngle;
                 float r = sqrt(float(i) / float(samples));
                 
                 vec2 offset = vec2(cos(theta), sin(theta)) * r * maxRadius * pixelSize;
                 
                 /* Sample Color */
                 vec3 sampleCol = texture(screenTexture, TexCoords + offset).rgb;
                 
                 /* Weighting: Uniforme pour un bokeh en forme de disque plat */
                 float weight = 1.0; 
                 
                 acc += sampleCol * weight;
                 totalWeight += weight;
            }
            
            color = acc / totalWeight;
        }
        
        /* Debug Visualization (Unreal Style) */
        /* Green = Near Field Blur */
        /* Blue = Far Field Blur */
        /* Black = In Focus */
        if (enableDoFDebug != 0) {
            vec3 debugColor = vec3(0.0); /* Black by default (In Focus) */
            
            if (dist < dofFocalDistance && blurAmount > 0.0) {
                /* Near Field - Green */
                debugColor = vec3(0.0, blurAmount, 0.0); 
            } else if (dist > dofFocalDistance && blurAmount > 0.0) {
                /* Far Field - Blue */
                debugColor = vec3(0.0, 0.0, blurAmount);
            }
            
            /* Blend with original mostly to see shapes, or just Replace? 
               Unreal replaces. Let's replace but keep it visualized by intensity. */
             color = debugColor;
        }
    }

    /* Appliquer le Bloom (Additif, avant exposition) */
    if (enableBloom != 0) {
        vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;
        color += bloomColor * bloomIntensity;
    }
    
    /* Appliquer l'exposition en premier (pour le HDR -> LDR) */
    float finalExposure = 1.0;
    if (enableAutoExposure != 0) {
        /* Lire l'exposition calculée par le Compute Shader */
        float autoExp = texture(autoExposureTexture, vec2(0.5)).r;
        /* Manual Exposure agit comme une compensation (EV bias) */
        finalExposure = autoExp * exposure;
    } else if (enableExposure != 0) {
        finalExposure = exposure;
    }
    
    color *= finalExposure;
    
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