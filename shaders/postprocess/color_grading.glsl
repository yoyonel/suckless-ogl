/* ============================================================================
   EFFECT: WHITE BALANCE
   ============================================================================
 */

/*
 * White Balance (Approximation rapide)
 * Basé sur une conversion analytique de température Kelvin/Teinte vers RGB
 */
vec3 applyWhiteBalance(vec3 color)
{
	/* Early exit if neutral (6500K, 0 tint) */
	if (abs(wb_temperature - 6500.0) < 1.0 && abs(wb_tint) < 0.001) {
		return color;
	}

	/* Temperature shift (simple linear approximation) */
	float tempShift = (wb_temperature - 6500.0) / 10000.0;

	vec3 wbColor = vec3(1.0);

	if (tempShift < 0.0) {
		/* Cooler (more blue) */
		wbColor.b = 1.0 - tempShift;
	} else {
		/* Warmer (more red/yellow) */
		wbColor.r = 1.0 + tempShift;
		wbColor.g = 1.0 + tempShift * 0.5;
	}

	/* Tint (Green/Magenta shift) */
	wbColor.g += wb_tint * 0.5;

	return color * wbColor;
}

/* ============================================================================
   EFFECT: COLOR GRADING
   ============================================================================
 */

/*
 * Logique Color Grading Unreal Engine
 */
vec3 apply_color_grading(vec3 color)
{
	/* 0. Apply White Balance First */
	color = applyWhiteBalance(color);

	/* 1. Saturation */
	float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
	color = mix(vec3(luminance), color, cg_saturation);

	/* 2. Contraste */
	color = (color - 0.5) * cg_contrast + 0.5;
	color = max(vec3(0.0), color);

	/* 3. Gamma */
	if (cg_gamma > 0.0) {
		color = pow(color, vec3(cg_gamma));
	}

	/* 4. Gain */
	color = color * cg_gain;

	/* 5. Offset */
	color = color + cg_offset;

	return max(vec3(0.0), color);
}
