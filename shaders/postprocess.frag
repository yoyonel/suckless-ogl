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

uniform sampler2D neighborMaxTexture;

/* Paramètres White Balance */
uniform float wbTemperature;
uniform float wbTint;

/* Paramètres Tonemapper */
uniform float tonemapSlope;
uniform float tonemapToe;
uniform float tonemapShoulder;
uniform float tonemapBlackClip;
uniform float tonemapWhiteClip;

/* Fonction de bruit pseudo-aléatoire pour le grain */
float random(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

/* Interleaved Gradient Noise for Dithering */
float InterleavedGradientNoise(vec2 position) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(position, magic.xy)));
}

/* Depth Linearization Helper */
float linearizeDepth(float d) {
    float zNear = 0.1;
    float zFar = 1000.0;
    float z_ndc = 2.0 * d - 1.0;
    return (2.0 * zNear * zFar) / (zFar + zNear - z_ndc * (zFar - zNear));
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

/* Advanced Reconstruction using NeighborMax and Depth Weighting */
vec3 applyMotionBlur(vec2 uv) {
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

    /* 2. Get Neighbor Max Velocity */
    vec2 maxNeighborVelocity = texture(neighborMaxTexture, uv).rg * motionBlurIntensity;
    float maxNeighborSpeed = length(maxNeighborVelocity);

    /* Fetch Center Color (Raw) */
    vec3 centerColor = texture(screenTexture, uv).rgb;

    /* Early exit if negligible motion */
    if (speed < 0.0001 && maxNeighborSpeed < 0.0001) {
       return centerColor;
    }

    /* Jitter */
    float noise = InterleavedGradientNoise(gl_FragCoord.xy);

    /* Center Depth */
    float centerDepth = linearizeDepth(texture(depthTexture, uv).r);

    vec3 acc = centerColor;
    float totalWeight = 1.0;

    int samples = motionBlurSamples;

    for (int i = 0; i < samples; ++i) {
        if (i == samples / 2) continue; // Skip center

        float t = mix(-0.5, 0.5, (float(i) + noise) / float(samples));
        vec2 sampleUV = uv + velocity * t;

        /* Always sample RAW screen texture here.
           (CA is applied *after* this function returns) */
        vec3 sampleColor = texture(screenTexture, sampleUV).rgb;

        /* Depth Weighting */
        float sampleDepth = linearizeDepth(texture(depthTexture, sampleUV).r);
        float depthDiff = sampleDepth - centerDepth;
        float weight = 1.0;

        if (depthDiff > 1.0) {
            weight = 0.1;
        } else if (depthDiff < -1.0) {
            weight = 1.0;
        } else {
            weight = 1.0;
        }

        acc += sampleColor * weight;
        totalWeight += weight;
    }

    return acc / totalWeight;
}

/* Wrapper to get "Scene Color" (Blurred or Raw) for CA to sample */
vec3 getSceneSource(vec2 uv) {
    if (enableMotionBlur != 0) {
        return applyMotionBlur(uv);
    }
    return texture(screenTexture, uv).rgb;
}

/* Effet Aberration Chromatique (Optimized: single Motion Blur call) */
vec3 applyChromAbbr(vec2 uv) {
    vec2 direction = uv - vec2(0.5);

    /* Get center pixel with motion blur (if enabled) */
    vec3 centerBlurred = getSceneSource(uv);

    /* Direct texture samples for R/B channels (skip motion blur for performance)
     * Trade-off: Only green channel gets motion blur, but CA is subtle at edges */
    float r = texture(screenTexture, uv + direction * chromAbbrStrength).r;
    float b = texture(screenTexture, uv - direction * chromAbbrStrength).b;

    return vec3(r, centerBlurred.g, b);
}

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
 */
vec3 unrealTonemap(vec3 x) {
    float a = 2.51 * tonemapSlope;
    const float b = 0.03;
    const float c = 2.43;
    float d = 0.59 * tonemapShoulder;
    float e = 0.14 * (1.1 - tonemapToe);

    vec3 res = (x * (a * x + b)) / (x * (c * x + d) + e);

    if (tonemapBlackClip > 0.001) {
        res = max(vec3(0.0), res - tonemapBlackClip) / (1.0 - tonemapBlackClip);
    }

    if (tonemapWhiteClip > 0.001) {
        float maxVal = 1.0 - tonemapWhiteClip;
        res = min(vec3(maxVal), res) / maxVal;
    }

    return clamp(res, 0.0, 1.0);
}

/*
 * Logique Color Grading Unreal Engine
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

void main() {
    /* 1. Priority Debug Check for Motion Blur */
    if (enableMotionBlurDebug != 0) {
        vec3 debugColor = applyMotionBlur(TexCoords);
        FragColor = vec4(debugColor, 1.0);
        return;
    }

    vec3 color;

    /* Skybox Hack: Depth ~ 1.0 */
    float depth = texture(depthTexture, TexCoords).r;
    bool isSkybox = depth >= 0.99999;

    /* 2. Pipeline: Motion Blur -> Chromatic Aberration */
    if (enableChromAbbr != 0 && !isSkybox) {
        /* CA samples "SceneSource" (which calls MB) */
        color = applyChromAbbr(TexCoords);
    } else {
        /* Direct fetch (or MB only) */
        color = getSceneSource(TexCoords);
    }

    /* --------------------------------------------------------
       Depth of Field (DoF) - "Cinematic" / Unreal Style Logic
       (Optimized with early exit)
       -------------------------------------------------------- */
    if (enableDoF != 0) {
        float depth = texture(depthTexture, TexCoords).r;

        /* Early exit for skybox (before any expensive calculations) */
        if (depth >= 0.99999) {
            /* Skip DoF for skybox */
        } else {
            float zNear = 0.1;
            float zFar = 1000.0;
            float z_ndc = 2.0 * depth - 1.0;
            float dist = (2.0 * zNear * zFar) / (zFar + zNear - z_ndc * (zFar - zNear));

            float coc = abs(dist - dofFocalDistance) / (dist + 0.0001);
            float blurAmount = clamp(coc * dofBokehScale, 0.0, 1.0);

            /* Apply focal range (in-focus zone) */
            if (dist > dofFocalDistance - dofFocalRange && dist < dofFocalDistance + dofFocalRange) {
                blurAmount = 0.0;
            } else {
                /* Smooth transition at edges of focal range */
                float edge = dofFocalRange;
                float distDiff = abs(dist - dofFocalDistance);
                if (distDiff < edge + 5.0) {
                    blurAmount *= (distDiff - edge) / 5.0;
                }
            }

            blurAmount = clamp(blurAmount, 0.0, 1.0);

            /* ⭐ CRITICAL OPTIMIZATION: Early exit before expensive blur loop */
            if (blurAmount > 0.01) {
                vec3 acc = vec3(0.0);
                float totalWeight = 0.0;
                float centerDepth = linearizeDepth(depth);
                float maxRadius = 10.0 * blurAmount;
                vec2 texSize = vec2(textureSize(screenTexture, 0));
                vec2 pixelSize = 1.0 / texSize;
                int samples = 16;
                float goldenAngle = 2.39996323;

                for(int i = 0; i < samples; i++) {
                     float theta = float(i) * goldenAngle;
                     float r = sqrt(float(i) / float(samples));
                     vec2 offset = vec2(cos(theta), sin(theta)) * r * maxRadius * pixelSize;
                     vec3 sampleCol = texture(screenTexture, TexCoords + offset).rgb;
                     float sampleDepth = linearizeDepth(texture(depthTexture, TexCoords + offset).r);
                     float depthDiff = sampleDepth - centerDepth;
                     float weight = 1.0;
                     if (depthDiff > 1.0) weight = 0.1;
                     else if (depthDiff < -1.0) weight = 1.0;
                     else weight = 1.0;
                     acc += sampleCol * weight;
                     totalWeight += weight;
                }
                color = acc / totalWeight;
            }

            /* Debug visualization */
            if (enableDoFDebug != 0) {
                vec3 debugColor = vec3(0.0);
                if (dist < dofFocalDistance && blurAmount > 0.0) debugColor = vec3(0.0, blurAmount, 0.0);
                else if (dist > dofFocalDistance && blurAmount > 0.0) debugColor = vec3(0.0, 0.0, blurAmount);
                color = debugColor;
            }
        }
    }

    /* Appliquer le Bloom (Additif, avant exposition) */
    if (enableBloom != 0) {
        vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;
        color += bloomColor * bloomIntensity;
    }

    /* Appliquer l'exposition */
    float finalExposure = 1.0;

    if (enableAutoExposure != 0) {
        /* Auto-exposure REPLACES manual exposure (no multiplication) */
        finalExposure = texture(autoExposureTexture, vec2(0.5)).r;
    } else if (enableExposure != 0) {
        /* Manual exposure only when auto-exposure is disabled */
        finalExposure = exposure;
    }

    color *= finalExposure;

    /* Appliquer White Balance */
    if (enableColorGrading != 0) {
        color = applyWhiteBalance(color);
    }

    /* Appliquer le color grading */
    if (enableColorGrading != 0) {
        color = apply_color_grading(color);
    }

    /* Tone Mapping */
    color = unrealTonemap(color);

    /* Appliquer le vignettage */
    if (enableVignette != 0) {
        color = applyVignette(color, TexCoords);
    }

    /* Correction Gamma */
    color = pow(color, vec3(1.0 / 2.2));

    /* Appliquer le grain */
    if (enableGrain != 0) {
        color = applyGrain(color, TexCoords);
    }

    FragColor = vec4(color, 1.0);
}
