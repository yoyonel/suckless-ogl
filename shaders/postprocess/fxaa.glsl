// FXAA 3.11 based implementation
// Optimized for Quality/Performance balance

// Quality Preset: 5 search steps (good quality, reasonable cost)
#define FXAA_QUALITY_PS 5
#define FXAA_QUALITY_P0 1.0
#define FXAA_QUALITY_P1 1.5
#define FXAA_QUALITY_P2 2.0
#define FXAA_QUALITY_P3 4.0
#define FXAA_QUALITY_P4 8.0

// Perceptual luma (linear, no sqrt — fast approximation)
float FxaaLuma(vec3 rgb)
{
	return dot(rgb, vec3(0.299, 0.587, 0.114));
}

vec3 applyFXAA(vec3 colorInput, vec2 texCoords)
{
	vec2 inverseScreenSize = screenTexelSize;

	// ------------------------------------------------------------------------
	// 1. Luma Analysis (Center + 4 Neighbors)
	//    All luma values are computed from screenTexture for coherence.
	//    colorInput (post-MB/CA) is only used as the early-exit return.
	// ------------------------------------------------------------------------
	vec3 rgbM = texture(screenTexture, texCoords).rgb;
	float lumaM = FxaaLuma(rgbM);

#ifdef USE_TRANSPARENT_BILLBOARDS
	float lumaN =
	    FxaaLuma(textureOffset(screenTexture, texCoords, ivec2(0, -1)).rgb);
	float lumaW =
	    FxaaLuma(textureOffset(screenTexture, texCoords, ivec2(-1, 0)).rgb);
	float lumaE =
	    FxaaLuma(textureOffset(screenTexture, texCoords, ivec2(1, 0)).rgb);
	float lumaS =
	    FxaaLuma(textureOffset(screenTexture, texCoords, ivec2(0, 1)).rgb);
#else
	float lumaN = textureOffset(screenTexture, texCoords, ivec2(0, -1)).a;
	float lumaW = textureOffset(screenTexture, texCoords, ivec2(-1, 0)).a;
	float lumaE = textureOffset(screenTexture, texCoords, ivec2(1, 0)).a;
	float lumaS = textureOffset(screenTexture, texCoords, ivec2(0, 1)).a;
#endif

	float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
	float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));
	float range = rangeMax - rangeMin;

	// Early Exit: Contrast too low?
	if (range < max(fxaaQualityEdgeThresholdMin,
	                rangeMax * fxaaQualityEdgeThreshold)) {
		return colorInput;
	}

	// ------------------------------------------------------------------------
	// 2. Corner Sampling (Diagonal neighbors)
	// ------------------------------------------------------------------------
#ifdef USE_TRANSPARENT_BILLBOARDS
	float lumaNW = FxaaLuma(
	    textureOffset(screenTexture, texCoords, ivec2(-1, -1)).rgb);
	float lumaNE =
	    FxaaLuma(textureOffset(screenTexture, texCoords, ivec2(1, -1)).rgb);
	float lumaSW =
	    FxaaLuma(textureOffset(screenTexture, texCoords, ivec2(-1, 1)).rgb);
	float lumaSE =
	    FxaaLuma(textureOffset(screenTexture, texCoords, ivec2(1, 1)).rgb);
#else
	float lumaNW = textureOffset(screenTexture, texCoords, ivec2(-1, -1)).a;
	float lumaNE = textureOffset(screenTexture, texCoords, ivec2(1, -1)).a;
	float lumaSW = textureOffset(screenTexture, texCoords, ivec2(-1, 1)).a;
	float lumaSE = textureOffset(screenTexture, texCoords, ivec2(1, 1)).a;
#endif

	// Filter Direction (Vertical vs Horizontal)
	float lumaL = (lumaN + lumaS + lumaE + lumaW) * 0.25;

	float edgeVert =
	    abs((0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE)) +
	    abs((0.50 * lumaW) + (-1.0 * lumaM) + (0.50 * lumaE)) +
	    abs((0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE));

	float edgeHorz =
	    abs((0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW)) +
	    abs((0.50 * lumaN) + (-1.0 * lumaM) + (0.50 * lumaS)) +
	    abs((0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE));

	bool isHorz = edgeHorz >= edgeVert;

	// ------------------------------------------------------------------------
	// 3. Sub-Pixel AA
	// ------------------------------------------------------------------------
	float subPixelOffset1 = clamp(abs(lumaL - lumaM) / range, 0.0, 1.0);
	float subPixelOffset2 = (-2.0 * subPixelOffset1) + 3.0;
	float subPixelOffsetFinal =
	    subPixelOffset1 * subPixelOffset1 * subPixelOffset2;
	subPixelOffsetFinal =
	    subPixelOffsetFinal * subPixelOffsetFinal * fxaaQualitySubpix;

	// ------------------------------------------------------------------------
	// 4. Edge Search
	// ------------------------------------------------------------------------
	float luma1 = isHorz ? lumaN : lumaW;
	float luma2 = isHorz ? lumaS : lumaE;
	float gradient1 = luma1 - lumaM;
	float gradient2 = luma2 - lumaM;

	bool is1Steepest = abs(gradient1) >= abs(gradient2);
	float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

	float stepLength = isHorz ? inverseScreenSize.y : inverseScreenSize.x;

	if (is1Steepest) {
		stepLength = -stepLength;
	}
	float lumaLocalAverage = 0.5 * ((is1Steepest ? luma1 : luma2) + lumaM);

	vec2 currentUv = texCoords;
	if (isHorz) {
		currentUv.y += stepLength * 0.5;
	} else {
		currentUv.x += stepLength * 0.5;
	}

	// Iterative edge search with variable step sizes
	vec2 offset = isHorz ? vec2(inverseScreenSize.x, 0.0)
	                     : vec2(0.0, inverseScreenSize.y);
	vec2 uv1 = currentUv - offset * FXAA_QUALITY_P0;
	vec2 uv2 = currentUv + offset * FXAA_QUALITY_P0;

	float lumaEnd1, lumaEnd2;
	bool reached1 = false;
	bool reached2 = false;

	const float quality[FXAA_QUALITY_PS] = float[FXAA_QUALITY_PS](
	    FXAA_QUALITY_P0, FXAA_QUALITY_P1, FXAA_QUALITY_P2, FXAA_QUALITY_P3,
	    FXAA_QUALITY_P4);

	/* First sample at ±P0 already done above. Loop advances further. */
	for (int i = 1; i < FXAA_QUALITY_PS; i++) {
		if (!reached1) {
#ifdef USE_TRANSPARENT_BILLBOARDS
			lumaEnd1 = FxaaLuma(texture(screenTexture, uv1).rgb);
#else
			lumaEnd1 = texture(screenTexture, uv1).a;
#endif
			lumaEnd1 -= lumaLocalAverage;
		}
		if (!reached2) {
#ifdef USE_TRANSPARENT_BILLBOARDS
			lumaEnd2 = FxaaLuma(texture(screenTexture, uv2).rgb);
#else
			lumaEnd2 = texture(screenTexture, uv2).a;
#endif
			lumaEnd2 -= lumaLocalAverage;
		}

		reached1 = abs(lumaEnd1) >= gradientScaled;
		reached2 = abs(lumaEnd2) >= gradientScaled;

		if (reached1 && reached2)
			break;

		if (!reached1)
			uv1 -= offset * quality[i];
		if (!reached2)
			uv2 += offset * quality[i];
	}

	// Distance Ratios
	float distance1 =
	    isHorz ? (texCoords.x - uv1.x) : (texCoords.y - uv1.y);
	float distance2 =
	    isHorz ? (uv2.x - texCoords.x) : (uv2.y - texCoords.y);

	bool isDirection1 = distance1 < distance2;
	float distanceFinal = min(distance1, distance2);
	float edgeThickness = (distance1 + distance2);
	float pixelOffset = -distanceFinal / edgeThickness + 0.5;

	// Overshoot check
	bool isLumaCenterSmaller = lumaM < lumaLocalAverage;
	bool correctVariation =
	    ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
	float finalOffset = correctVariation ? pixelOffset : 0.0;

	// Blend with subpixel
	finalOffset = max(finalOffset, subPixelOffsetFinal);

	// ------------------------------------------------------------------------
	// 5. Final Read & Output
	// ------------------------------------------------------------------------
	vec2 finalUv = texCoords;
	if (isHorz) {
		finalUv.y += finalOffset * stepLength;
	} else {
		finalUv.x += finalOffset * stepLength;
	}

	// ------------------------------------------------------------------------
	// 6. Debug Visualization
	// ------------------------------------------------------------------------
	if (enableFXAADebug) {
		if (finalOffset > 0.001) {
			if (subPixelOffsetFinal > finalOffset * 0.9) {
				return vec3(0.1, 0.4,
				            1.0); /* Subpixel (Blue) */
			}
			return vec3(1.0, 0.2, 0.2); /* Edge (Red) */
		}
		float gray = FxaaLuma(rgbM);
		return vec3(gray * 0.5); /* Untouched (Grayscale) */
	}

	/* Read the anti-aliased result from the offset position.
	 * Note: FXAA operates on screenTexture (pre-MB/CA) because it needs
	 * coherent neighbor access. The MB/CA result from colorInput is only
	 * returned on early exit (no edge detected). */
	return texture(screenTexture, finalUv).rgb;
}
