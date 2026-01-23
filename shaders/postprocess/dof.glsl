/* Paramètres DoF */
uniform int enableDoF; /* Flag d'activation DoF */
uniform int enableDoFDebug; /* Flag de debug DoF */
uniform float dofFocalDistance;
uniform float dofFocalRange;
uniform float dofBokehScale;

/* ============================================================================
   EFFECT: DEPTH OF FIELD
   ============================================================================ */

vec3 applyDoF(vec3 color, vec2 uv) {
    float depth = texture(depthTexture, uv).r;

    /* Early exit for skybox (before any expensive calculations) */
    if (depth >= 0.99999) {
        /* Skip DoF for skybox */
        return color;
    }

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
             vec3 sampleCol = texture(screenTexture, uv + offset).rgb;
             float sampleDepth = linearizeDepth(texture(depthTexture, uv + offset).r);
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
        return debugColor;
    }

    return color;
}
