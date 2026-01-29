// FXAA 3.11 based implementation
// Optimized for Quality/Performance balance

// Settings provided by PostProcessUBO
// FXAA_MODE: 0 = Performance (Console), 1 = Quality (PC)
#ifndef FXAA_MODE
#define FXAA_MODE 1
#endif

// Quality/Performance Tuning
#if FXAA_MODE == 1
#define FXAA_QUALITY_PS 5
// Steps defined as array-like access or macros
#define FXAA_QUALITY_P0 1.0
#define FXAA_QUALITY_P1 1.5
#define FXAA_QUALITY_P2 2.0
#define FXAA_QUALITY_P3 2.0
#define FXAA_QUALITY_P4 2.0
#define FXAA_QUALITY_P5 8.0
#else
// Performance constants
#define FXAA_QUALITY_PS 0  // No loop
#endif

// Helper for Luma calculation on the fly (Center Pixel)
// We use sqrt to approximate Gamma correction for better perceptual edge
// detection
float FxaaLuma(vec3 rgb)
{
	return dot(sqrt(rgb), vec3(0.299, 0.587, 0.114));
}

vec3 applyFXAA(vec3 colorInput, vec2 texCoords)
{
	vec2 inverseScreenSize = 1.0 / textureSize(screenTexture, 0);

	// ------------------------------------------------------------------------
	// 1. Luma Analysis (Center + 4 Neighbors)
	// ------------------------------------------------------------------------

	// Center Luma: Calculate from input color (which might be processed by
	// other effects)
	float lumaM = FxaaLuma(colorInput);

	// Neighbor Luma: Fetch directly from Alpha channel (Pre-computed in PBR
	// pass) We use textureOffset for cleaner/faster immediate neighbor
	// access
	float lumaN = textureOffset(screenTexture, texCoords, ivec2(0, -1)).a;
	float lumaW = textureOffset(screenTexture, texCoords, ivec2(-1, 0)).a;
	float lumaE = textureOffset(screenTexture, texCoords, ivec2(1, 0)).a;
	float lumaS = textureOffset(screenTexture, texCoords, ivec2(0, 1)).a;

	float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
	float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));
	float range = rangeMax - rangeMin;

	// Early Exit: Contrast too low?
	// 0.063 = EdgeThresholdMin, 0.125 = EdgeThreshold
	if (range < max(0.063, rangeMax * 0.125)) {
		return colorInput;
	}

	// ------------------------------------------------------------------------
	// 2. Corner Sampling (Neighbors of neighbors)
	// ------------------------------------------------------------------------
	float lumaNW = textureOffset(screenTexture, texCoords, ivec2(-1, -1)).a;
	float lumaNE = textureOffset(screenTexture, texCoords, ivec2(1, -1)).a;
	float lumaSW = textureOffset(screenTexture, texCoords, ivec2(-1, 1)).a;
	float lumaSE = textureOffset(screenTexture, texCoords, ivec2(1, 1)).a;

	// Filter Direction (Vertical vs Horizontal)
	float lumaL = (lumaN + lumaS + lumaE + lumaW) * 0.25;
	float rangeL = abs(lumaL - lumaM);
	float blendL = max(0.0, (rangeL / range) - 0.0);

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
	// 3. Sub-Pixel AA (Common to both modes)
	// ------------------------------------------------------------------------
	float subPixelOffset1 = clamp(abs(lumaL - lumaM) / range, 0.0, 1.0);
	float subPixelOffset2 = (-2.0 * subPixelOffset1) + 3.0;
	float subPixelOffsetFinal =
	    subPixelOffset1 * subPixelOffset1 * subPixelOffset2;
	subPixelOffsetFinal =
	    subPixelOffsetFinal * subPixelOffsetFinal * 0.75;  // Subpix Quality

	// ------------------------------------------------------------------------
	// 4. Edge Search (Mode Dependent)
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
	// Average Luma at the edge border
	float lumaLocalAverage = 0.5 * ((is1Steepest ? luma1 : luma2) + lumaM);

	vec2 currentUv = texCoords;
	if (isHorz) {
		currentUv.y += stepLength * 0.5;
	} else {
		currentUv.x += stepLength * 0.5;
	}

#if FXAA_MODE == 0
	// --- PERFORMANCE MODE (Console) ---
	float finalOffset = subPixelOffsetFinal;

#else
	// --- QUALITY MODE (PC) ---
	// Iterative loop to find end of edge using Variable Steps
	vec2 offset = isHorz ? vec2(inverseScreenSize.x, 0.0)
	                     : vec2(0.0, inverseScreenSize.y);
	vec2 uv1 = currentUv - offset * FXAA_QUALITY_P0;
	vec2 uv2 = currentUv + offset * FXAA_QUALITY_P0;

	float lumaEnd1, lumaEnd2;
	bool reached1 = false;
	bool reached2 = false;
	bool reachedBoth = false;

	// We unroll or use array for steps. Here a manual switch/array is safer
	// or just if-cascade inside loop if compiler unrolls. Let's use a small
	// array for step multipliers
	float quality[5];
	quality[0] = FXAA_QUALITY_P1;
	quality[1] = FXAA_QUALITY_P2;
	quality[2] = FXAA_QUALITY_P3;
	quality[3] = FXAA_QUALITY_P4;
	quality[4] = FXAA_QUALITY_P5;

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

		if (reachedBoth)
			break;

		// Advance using variable step
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

	// Check overshoot
	bool isLumaCenterSmaller = lumaM < lumaLocalAverage;
	bool correctVariation =
	    ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
	float finalOffset = correctVariation ? pixelOffset : 0.0;

	// Blend with subpixel
	finalOffset = max(finalOffset, subPixelOffsetFinal);
#endif

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
		// Red = Edge displacement
		// Blue = Subpixel displacement
		if (finalOffset > 0.001) {
			if (subPixelOffsetFinal > finalOffset * 0.9) {
				return vec3(0.1, 0.4, 1.0);  // Blueish (Subpix)
			}
			return vec3(1.0, 0.2, 0.2);  // Reddish (Edge)
		}
		// Show untouched pixels in grayscale
		vec3 original = texture(screenTexture, texCoords).rgb;
		float gray = dot(original, vec3(0.3, 0.59, 0.11));
		return vec3(gray * 0.5);
	}

	return texture(screenTexture, finalUv).rgb;
}
