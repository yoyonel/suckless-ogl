/*
 * BANDING / QUANTIZATION EFFECT
 * Advanced styles for stylized color reduction.
 */

// Bayer 4x4 matrix for ordered dithering
const float bayer4x4[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
                                   3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

/**
 * Quantize a single value to N levels.
 */
vec3 quantize(vec3 color, vec3 levels)
{
	return floor(color * levels) / levels;
}

/**
 * Main switch for banding styles.
 */
vec3 applyBanding(vec3 color)
{
	vec3 result = color;

	// Modes
	// 0: Linear (Posterization)
	// 1: Dithered (Retro Computing)
	// 2: Perceptual (Analog style)
	// 3: Channel-specific (CGA/VGA style)
	// 4: Luminance (Blueprint style)

	if (bandingMode == 0) {  // LINEAR / POSTERIZATION
		float levels = max(1.0, bandingLevels);
		result = quantize(color, vec3(levels));
	} else if (bandingMode == 1) {  // DITHERED
		float levels = max(1.0, bandingLevels);

		// Calculate screen-space coordinates for Bayer lookup
		ivec2 pixelPos = ivec2(gl_FragCoord.xy) % 4;
		float bayerValue =
		    (bayer4x4[pixelPos.y * 4 + pixelPos.x] / 16.0) - 0.5;

		// Apply dither noise before quantization
		vec3 ditheredColor =
		    color + (bayerValue * bandingDitherStrength / levels);
		result = quantize(clamp(ditheredColor, 0.0, 1.0), vec3(levels));
	} else if (bandingMode == 2) {  // PERCEPTUAL
		float levels = max(1.0, bandingLevels);
		float gamma = max(0.1, bandingPerceptualGamma);

		// Quantize in a non-linear space
		result = pow(color, vec3(gamma));
		result = quantize(result, vec3(levels));
		result = pow(result, vec3(1.0 / gamma));
	} else if (bandingMode == 3) {  // CHANNEL-SPECIFIC
		vec3 levels = max(vec3(1.0), bandingChannelLevels);
		result = quantize(color, levels);
	} else if (bandingMode == 4) {  // LUMINANCE (Blueprint)
		float lum = dot(color, vec3(0.299, 0.587, 0.114));
		float levels = max(1.0, bandingLevels);
		float quantizedLum = floor(lum * levels) / levels;

		// Tint based on channel_levels
		result = quantizedLum * bandingChannelLevels;
	}

	return result;
}
