// FXAA 3.11 based implementation
// Optimized for Quality/Performance balance

// Settings provided by PostProcessUBO

#ifndef FXAA_QUALITY_PRESET
#define FXAA_QUALITY_PRESET 12
#endif

// Settings based on preset
#if (FXAA_QUALITY_PRESET == 10)
#define FXAA_QUALITY_PS 3
#define FXAA_QUALITY_P0 1.5
#define FXAA_QUALITY_P1 3.0
#define FXAA_QUALITY_P2 12.0
#elif (FXAA_QUALITY_PRESET == 12)
#define FXAA_QUALITY_PS 5
#define FXAA_QUALITY_P0 1.0
#define FXAA_QUALITY_P1 1.5
#define FXAA_QUALITY_P2 2.0
#define FXAA_QUALITY_P3 2.0
#define FXAA_QUALITY_P4 2.0
#define FXAA_QUALITY_P5 8.0
#endif

/* Simple Luminance Calculation */
float FxaaLuma(vec3 rgb)
{
	return dot(rgb, vec3(0.299, 0.587, 0.114));
}

vec3 applyFXAA(vec3 colorInput, vec2 texCoords)
{
	vec2 inverseScreenSize = 1.0 / textureSize(screenTexture, 0);

	// Sample center and 4 neighbors for edge detection
	vec3 rgbM = colorInput;  // Center is passed in, or re-sample?
	// Let's re-sample to be safe and consistent with FXAA logic which needs
	// exact texel centers Actually, FXAA needs samples around the center.

	vec3 rgbN = texture(screenTexture,
	                    texCoords + vec2(0.0, -1.0) * inverseScreenSize)
	                .rgb;
	vec3 rgbW = texture(screenTexture,
	                    texCoords + vec2(-1.0, 0.0) * inverseScreenSize)
	                .rgb;
	vec3 rgbE = texture(screenTexture,
	                    texCoords + vec2(1.0, 0.0) * inverseScreenSize)
	                .rgb;
	vec3 rgbS = texture(screenTexture,
	                    texCoords + vec2(0.0, 1.0) * inverseScreenSize)
	                .rgb;

	float lumaM = FxaaLuma(rgbM);
	float lumaN = FxaaLuma(rgbN);
	float lumaW = FxaaLuma(rgbW);
	float lumaE = FxaaLuma(rgbE);
	float lumaS = FxaaLuma(rgbS);

	float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
	float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));
	float range = rangeMax - rangeMin;

	// 1. Early Exit: No edge detected or contrast too low
	if (range <
	    max(0.063, rangeMax * 0.125)) {  // 0.063 = EdgeThresholdMin, 0.125
		                             // = EdgeThreshold
		return rgbM;
	}

	// 2. Luma calculation for corners to refine edge direction
	vec3 rgbNW = texture(screenTexture,
	                     texCoords + vec2(-1.0, -1.0) * inverseScreenSize)
	                 .rgb;
	vec3 rgbNE = texture(screenTexture,
	                     texCoords + vec2(1.0, -1.0) * inverseScreenSize)
	                 .rgb;
	vec3 rgbSW = texture(screenTexture,
	                     texCoords + vec2(-1.0, 1.0) * inverseScreenSize)
	                 .rgb;
	vec3 rgbSE = texture(screenTexture,
	                     texCoords + vec2(1.0, 1.0) * inverseScreenSize)
	                 .rgb;

	float lumaNW = FxaaLuma(rgbNW);
	float lumaNE = FxaaLuma(rgbNE);
	float lumaSW = FxaaLuma(rgbSW);
	float lumaSE = FxaaLuma(rgbSE);

	// 3. Determine Edge Direction (Vertical/Horizontal)
	float lumaL = (lumaN + lumaS + lumaE + lumaW) * 0.25;
	float rangeL = abs(lumaL - lumaM);
	float blendL =
	    max(0.0, (rangeL / range) - 0.0);  // Subpix quality would go here

	float edgeVert =
	    abs((0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE)) +
	    abs((0.50 * lumaW) + (-1.0 * lumaM) + (0.50 * lumaE)) +
	    abs((0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE));

	float edgeHorz =
	    abs((0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW)) +
	    abs((0.50 * lumaN) + (-1.0 * lumaM) + (0.50 * lumaS)) +
	    abs((0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE));

	bool isHorz = edgeHorz >= edgeVert;

	// 4. Gradient Calculation
	float luma1 = isHorz ? lumaN : lumaW;
	float luma2 = isHorz ? lumaS : lumaE;
	float gradient1 = luma1 - lumaM;
	float gradient2 = luma2 - lumaM;

	bool is1Steepest = abs(gradient1) >= abs(gradient2);
	float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

	float stepLength = isHorz ? inverseScreenSize.y : inverseScreenSize.x;
	float lumaLocalAverage = 0.0;

	if (is1Steepest) {
		stepLength = -stepLength;
		lumaLocalAverage = 0.5 * (luma1 + lumaM);
	} else {
		lumaLocalAverage = 0.5 * (luma2 + lumaM);
	}

	vec2 currentUv = texCoords;
	if (isHorz) {
		currentUv.y += stepLength * 0.5;
	} else {
		currentUv.x += stepLength * 0.5;
	}

	// 5. End-of-edge Search
	vec2 offset = isHorz ? vec2(inverseScreenSize.x, 0.0)
	                     : vec2(0.0, inverseScreenSize.y);
	vec2 uv1 = currentUv - offset;
	vec2 uv2 = currentUv + offset;

	float lumaEnd1, lumaEnd2;
	bool reached1 = false;
	bool reached2 = false;
	bool reachedBoth = false;

	for (int i = 0; i < FXAA_QUALITY_PS; i++) {
		if (!reached1) {
			lumaEnd1 = FxaaLuma(texture(screenTexture, uv1).rgb);
			lumaEnd1 = lumaEnd1 - lumaLocalAverage;
		}
		if (!reached2) {
			lumaEnd2 = FxaaLuma(texture(screenTexture, uv2).rgb);
			lumaEnd2 = lumaEnd2 - lumaLocalAverage;
		}

		reached1 = abs(lumaEnd1) >= gradientScaled;
		reached2 = abs(lumaEnd2) >= gradientScaled;
		reachedBoth = reached1 && reached2;

		if (!reached1) {
			uv1 -= offset;  // * Quality step ?
		}
		if (!reached2) {
			uv2 += offset;
		}

		if (reachedBoth)
			break;
	}

	// 6. Blending
	float distance1 =
	    isHorz ? (texCoords.x - uv1.x) : (texCoords.y - uv1.y);
	float distance2 =
	    isHorz ? (uv2.x - texCoords.x) : (uv2.y - texCoords.y);

	bool isDirection1 = distance1 < distance2;
	float distanceFinal = min(distance1, distance2);
	float edgeThickness = (distance1 + distance2);
	float pixelOffset = -distanceFinal / edgeThickness + 0.5;

	// Check if luma sign mismatch (overshoot)
	bool isLumaCenterSmaller = lumaM < lumaLocalAverage;
	bool correctVariation =
	    ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
	float finalOffset = correctVariation ? pixelOffset : 0.0;

	// Subpixel anti-aliasing (reduce flicker on single pixels)
	// Low quality = 0.0, High = 0.75 or 1.0 (fxaaQualitySubpix)
	float subPixelOffset1 = clamp(abs(lumaL - lumaM) / range, 0.0, 1.0);
	float subPixelOffset2 =
	    (-2.0 * subPixelOffset1) + 3.0;  // Smootherstep-ish
	float subPixelOffsetFinal =
	    subPixelOffset1 * subPixelOffset1 * subPixelOffset2;
	subPixelOffsetFinal = subPixelOffsetFinal * subPixelOffsetFinal *
	                      0.75;  // 0.75 = Default Subpix Quality

	finalOffset = max(finalOffset, subPixelOffsetFinal);

	// Final Read
	vec2 finalUv = texCoords;
	if (isHorz) {
		finalUv.y += finalOffset * stepLength;
	} else {
		finalUv.x += finalOffset * stepLength;
	}

	/* Debug Visualization */
	if (enableFXAADebug) {
		// Darken original image to make debug pop
		// Red = Affected pixels (Significant displacement)
		// Blue = Subpixel (Micro-displacement)
		if (finalOffset > 0.0) {
			if (subPixelOffsetFinal > finalOffset * 0.9) {
				return vec3(0.0, 0.5, 1.0);  // Subpixel (Blue)
			}
			return vec3(1.0, 0.0, 0.0);  // Edge (Red)
		}
		// Green tint for searched edges but no displacement?
		// Just grayscale for unaffected.
		vec3 original = texture(screenTexture, texCoords).rgb;
		return vec3(dot(original, vec3(0.3, 0.59, 0.11)));
	}

	return texture(screenTexture, finalUv).rgb;
}
