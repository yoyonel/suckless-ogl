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

/* Paramètres Motion Blur */
uniform sampler2D velocityTexture;
uniform float motionBlurIntensity;
uniform float motionBlurMaxVelocity;
uniform int motionBlurSamples;
uniform int enableMotionBlur;
uniform int enableMotionBlurDebug;

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
    /* Scale UV by texel size - larger texelSize = coarser grain */
    vec2 grainUV = uv / grainTexelSize;

    /* Add time offset to animate */
    float noise = random(grainUV + vec2(time)) * 2.0 - 1.0;

    /* 4. Apply Grain */
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

/* Paramètres White Balance */
uniform float wbTemperature;
uniform float wbTint;

/* Paramètres Tonemapper */
uniform float tonemapSlope;
uniform float tonemapToe;
uniform float tonemapShoulder;
uniform float tonemapBlackClip;
uniform float tonemapWhiteClip;

/*
 * White Balance (Approximation rapide)
 * Basé sur une conversion analytique de température Kelvin/Teinte vers RGB
 */
vec3 applyWhiteBalance(vec3 color) {
    /* Early exit if neutral (6500K, 0 tint) */
    if (abs(wbTemperature - 6500.0) < 1.0 && abs(wbTint) < 0.001) {
        return color;
    }

    /* Temperature shift (simple linear approximation) */
    float tempShift = (wbTemperature - 6500.0) / 10000.0;

    vec3 wb = vec3(1.0);

    if (tempShift < 0.0) {
        /* Cooler (more blue) */
        wb.b = 1.0 - tempShift;
    } else {
        /* Warmer (more red/yellow) */
        wb.r = 1.0 + tempShift;
        wb.g = 1.0 + tempShift * 0.5;
    }

    /* Tint (Green/Magenta shift) */
    wb.g += wbTint * 0.5;

    return color * wb;
}

/*
 * Filmic Tonemapper (Type Unreal / Hable / ACES modifié)
 * Courbe sigmoïde paramétrique :
 * x * (a*x + b) / (x * (c*x + d) + e)
 *
 * Coefficients dérivés de Slope/Toe/Shoulder sont complexes à calculer au runtime.
 * On utilise ici l'approximation directe "AMD Tonemapper" ou similaire qui map directement les params.
 *
 * Pour simplifier et matcher le look "Unreal", on garde la courbe ACES Narkowicz mais on module
 * l'entrée et la sortie avec les paramètres Slope (contraste) et Toe (offset).
 */
vec3 unrealTonemap(vec3 x) {
    /* ACES Standard (Narkowicz) - Apply first */
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    vec3 res = (x * (a * x + b)) / (x * (c * x + d) + e);

    /* Post-tonemap adjustments (all optional, defaults are neutral) */

    /* Black Clip - Crush blacks by remapping [clip, 1.0] to [0.0, 1.0] */
    if (tonemapBlackClip > 0.001) {
        res = max(vec3(0.0), res - tonemapBlackClip) / (1.0 - tonemapBlackClip);
    }

    /* White Clip - Compress highlights */
    if (tonemapWhiteClip > 0.001) {
        float maxVal = 1.0 - tonemapWhiteClip;
        res = min(vec3(maxVal), res) / maxVal;
    }

    return clamp(res, 0.0, 1.0);
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
    color = (color - 0.5) * gradContrast + 0.5;
    color = max(vec3(0.0), color);

    /* 3. Gamma */
    if (gradGamma > 0.0) {
        color = pow(color, vec3(gradGamma));
    }

    /* 4. Gain */
    color = color * gradGain;

    /* 5. Offset */
    color = color + gradOffset;

    return max(vec3(0.0), color);
}

/* Depth Linearization Helper */
float linearizeDepth(float d) {
    float zNear = 0.1;
    float zFar = 1000.0;
    float z_ndc = 2.0 * d - 1.0;
    return (2.0 * zNear * zFar) / (zFar + zNear - z_ndc * (zFar - zNear));
}

/* Interleaved Gradient Noise for Dithering */
float InterleavedGradientNoise(vec2 position) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(position, magic.xy)));
}

uniform sampler2D neighborMaxTexture;

/* Advanced Reconstruction using NeighborMax and Depth Weighting */
vec3 applyMotionBlur(vec3 color, vec2 uv) {
    /* 1. Get Velocity at center pixel */
    vec2 velocity = texture(velocityTexture, uv).rg;

    /* Debug Visualization (Early Exit) */
    if (enableMotionBlurDebug != 0) {
        return vec3(abs(velocity.x) * 20.0, abs(velocity.y) * 20.0, 0.0);
    }

    velocity *= motionBlurIntensity;

    /* Clamp main velocity */
    float speed = length(velocity);
    if (speed > motionBlurMaxVelocity) {
        velocity = normalize(velocity) * motionBlurMaxVelocity;
        speed = motionBlurMaxVelocity;
    }

    /* 2. Get Neighbor Max Velocity (for classification/clamping) */
    vec2 maxNeighborVelocity = texture(neighborMaxTexture, uv).rg * motionBlurIntensity;
    float maxNeighborSpeed = length(maxNeighborVelocity);

    /* Early exit if negligible motion (both local and neighborhood) */
    if (speed < 0.0001 && maxNeighborSpeed < 0.0001) {
       return color;
    }

    /* Use the dominant velocity in the region to guide the blur direction if local velocity is small?
       McGuire suggests checking if we are "background" vs "foreground".
       Simplified Logic: Use local velocity for direction, but clamp influence if neighbor is much slower?
       Actually, standard simple reconstruction uses just local velocity usually, OR dominant.
       Let's stick to local velocity for the path, but potentially use NeighborMax to widen the search or handle clamping.
       For this implementation, let's keep it robust: Use local velocity. NeighborMax is mostly for TileMax optimization (which we did in compute).
    */

    /* Jitter for dithering (0.0 to 1.0) */
    float noise = InterleavedGradientNoise(gl_FragCoord.xy);

    /* Get center depth for comparison */
    float centerDepth = linearizeDepth(texture(depthTexture, uv).r);

    vec3 acc = color;
    float totalWeight = 1.0;

    int samples = motionBlurSamples;

    for (int i = 0; i < samples; ++i) {
        if (i == samples / 2) continue; // Skip center

        /* Map i to [-0.5, 0.5] range, jittered */
        float t = mix(-0.5, 0.5, (float(i) + noise) / float(samples));

        vec2 sampleUV = uv + velocity * t;
        vec3 sampleColor = texture(screenTexture, sampleUV).rgb;

        /* Depth-aware weighting to prevent background bleeding over foreground */
        float sampleDepth = linearizeDepth(texture(depthTexture, sampleUV).r);

        /* If sample is significantly behind center (larger depth = farther), reduce weight
           This prevents background blur from contaminating sharp foreground edges */
        float depthDiff = sampleDepth - centerDepth;
        float weight = 1.0;

        if (depthDiff > 1.0) {
            /* Sample is behind center - reduce contribution */
            weight = 0.1;
        } else if (depthDiff < -1.0) {
            /* Sample is in front - keep full weight (foreground can blur over background) */
            weight = 1.0;
        } else {
            /* Similar depth - full weight */
            weight = 1.0;
        }

        acc += sampleColor * weight;
        totalWeight += weight;
    }

    return acc / totalWeight;
}

void main() {
/* 1. Priority Debug Check for Motion Blur */
    if (enableMotionBlurDebug != 0) {
        /* Visualize Velocity: Red = X, Green = Y */
        /* Note: applyMotionBlur will return debug color immediately if debug is on */
        /* but we need to pass a dummy color. The UVs matter. */
        vec3 debugColor = applyMotionBlur(vec3(0.0), TexCoords);
        FragColor = vec4(debugColor, 1.0);
        return;
    }

    vec3 color;

    /* Skybox Hack: Depth ~ 1.0 */
    float depth = texture(depthTexture, TexCoords).r;
    bool isSkybox = depth >= 0.99999;

    /* 2. Base Color (Standard or Chromatic Aberration) */
    if (enableChromAbbr != 0 && !isSkybox) {
        color = applyChromAbbr(TexCoords);
    } else {
        color = texture(screenTexture, TexCoords).rgb;
    }

    /* 3. Apply Motion Blur (Normal Mode) */
    if (enableMotionBlur != 0) {
        color = applyMotionBlur(color, TexCoords);
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

            /* Get center depth for comparison */
            float centerDepth = linearizeDepth(depth);

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

                 /* Sample Color and Depth */
                 vec3 sampleCol = texture(screenTexture, TexCoords + offset).rgb;
                 float sampleDepth = linearizeDepth(texture(depthTexture, TexCoords + offset).r);

                 /* Depth-aware weighting to prevent background bleeding over foreground */
                 float depthDiff = sampleDepth - centerDepth;
                 float weight = 1.0;

                 if (depthDiff > 1.0) {
                     /* Sample is behind center - reduce contribution */
                     weight = 0.1;
                 } else if (depthDiff < -1.0) {
                     /* Sample is in front - keep full weight */
                     weight = 1.0;
                 } else {
                     /* Similar depth - full weight */
                     weight = 1.0;
                 }

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

    /* Appliquer White Balance (avant color grading) */
    if (enableColorGrading != 0) {
        color = applyWhiteBalance(color);
    }

    /* Appliquer le color grading */
    if (enableColorGrading != 0) {
        color = apply_color_grading(color);
    }

    /* Tone Mapping (HDR -> LDR Linear) */
    /* Tone Mapping (HDR -> LDR Linear) */
    color = unrealTonemap(color);

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
