layout(binding = 8) uniform sampler3D u_lut_tex;

/**
 * @brief Applies 3D LUT gamut mapping to the color.
 * @param color Input linear HDR or Log color.
 * @return Transformed color.
 */
vec3 apply_lut3d(vec3 color)
{
	/*
	   3D LUT sampling. Coordinates are (r, g, b).
	   Note: LUTs expect [0, 1] input range.
	 */
	vec3 lut_color = texture(u_lut_tex, clamp(color, 0.0, 1.0)).rgb;
	return mix(color, lut_color, lut3d_intensity);
}
