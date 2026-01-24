/* Paramètres Motion Blur */
uniform sampler2D velocityTexture;
struct MotionBlurParams {
    float intensity;
    float maxVelocity;
    int samples;
};
uniform MotionBlurParams motionBlur;
uniform int enableMotionBlur;
uniform int enableMotionBlurDebug;

uniform sampler2D neighborMaxTexture;

/* ============================================================================
   EFFECT: MOTION BLUR
   ============================================================================ */

/* Advanced Reconstruction using NeighborMax and Depth Weighting */
vec3 applyMotionBlur(vec2 uv) {
    /* 1. Get Velocity at center pixel */
    vec2 velocity = texture(velocityTexture, uv).rg;

    /* Debug Visualization (Early Exit) */
    if (enableMotionBlurDebug != 0) {
        return vec3(abs(velocity.x) * 20.0, abs(velocity.y) * 20.0, 0.0);
    }

    velocity *= motionBlur.intensity;

    /* Clamp main velocity */
    float speed = length(velocity);
    if (speed > motionBlur.maxVelocity) {
        velocity = normalize(velocity) * motionBlur.maxVelocity;
        speed = motionBlur.maxVelocity;
    }

    /* 2. Get Neighbor Max Velocity */
    vec2 maxNeighborVelocity = texture(neighborMaxTexture, uv).rg * motionBlur.intensity;
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

    int samples = motionBlur.samples;

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
