/* ============================================================================
   EFFECT: TONEMAPPING
   ============================================================================
 */

/*
 * Filmic Tonemapper (Type Unreal / Hable / ACES modifié)
 */
vec3 unrealTonemap(vec3 x)
{
	float a = 2.51 * tm_slope;
	const float b = 0.03;
	const float c = 2.43;
	float d = 0.59 * tm_shoulder;
	float e = 0.14 * (1.1 - tm_toe);

	vec3 res = (x * (a * x + b)) / (x * (c * x + d) + e);

	if (tm_blackClip > 0.001) {
		res = max(vec3(0.0), res - tm_blackClip) / (1.0 - tm_blackClip);
	}

	if (tm_whiteClip > 0.001) {
		float maxVal = 1.0 - tm_whiteClip;
		res = min(vec3(maxVal), res) / maxVal;
	}

	return clamp(res, 0.0, 1.0);
}
