#version 450 core

out vec4 FragColor;

in vec2 TexCoords;

uniform int u_mode;

const float PI = 3.14159265359;

// Siemens Star pattern
float siemensStar(vec2 uv, float segments)
{
	vec2 rel = uv - 0.5;
	float angle = atan(rel.y, rel.x);
	float d = length(rel);

	// Antialiasing for the star itself is NOT desired here,
	// as we want to test the post-process AA.
	float pattern = step(0.0, sin(angle * segments));

	// Fade out towards the center to create extreme frequency
	float centerMask = smoothstep(0.0, 0.01, d);
	return pattern * centerMask;
}

// Rotated Grid pattern
float rotatedGrid(vec2 uv, float angle, float thickness)
{
	float s = sin(angle);
	float c = cos(angle);
	mat2 rot = mat2(c, -s, s, c);

	vec2 rotatedUV = rot * (uv * 20.0);
	vec2 grid = abs(fract(rotatedUV - 0.5) - 0.5);
	float line = min(grid.x, grid.y);

	return step(thickness, line);
}

void main()
{
	vec2 uv = TexCoords;
	vec3 finalColor = vec3(0.0);

	if (u_mode == 0) {
		// Siemens Star - Highly squashed and high frequency
		vec2 starUV = vec2(uv.x * 4.0, uv.y);
		finalColor = vec3(siemensStar(starUV, 128.0));
	} else if (u_mode == 1) {
		// Rotated Grid
		finalColor = vec3(rotatedGrid(uv, 15.0 * PI / 180.0, 0.02));
	} else if (u_mode == 2) {
		// User-Requested Spheres Alignment (8x8 Overlapping Grid)
		// Mimicking the overlapping look of the reference image with
		// high edge density
		vec3 bg = vec3(0.01, 0.01, 0.02);
		finalColor = bg;

		// 8 columns and 8 rows of spheres
		for (int x_idx = 0; x_idx < 8; x_idx++) {
			for (int y_idx = 0; y_idx < 8; y_idx++) {
				float tx = float(x_idx) / 7.0;
				float ty = float(y_idx) / 7.0;

				vec2 center =
				    vec2(0.1 + tx * 0.8, 0.1 + ty * 0.8);
				float r = 0.08;

				float dist = distance(uv, center);
				if (dist < r) {
					// High-contrast primary colors
					int color_idx = (x_idx + y_idx) % 4;
					vec3 base;
					if (color_idx == 0)
						base =
						    vec3(0.9, 0.1, 0.1);  // Red
					else if (color_idx == 1)
						base = vec3(0.1, 0.9,
						            0.1);  // Green
					else if (color_idx == 2)
						base = vec3(0.1, 0.1,
						            0.9);  // Blue
					else
						base = vec3(0.9, 0.9,
						            0.1);  // Yellow

					// Simple 3D shading
					float z = sqrt(
					    max(0.0, r * r - dot(uv - center,
					                         uv - center)));
					vec3 normal =
					    normalize(vec3(uv - center, z));
					float diff = max(
					    0.4,
					    dot(normal, normalize(vec3(1.0, 1.0,
					                               2.0))));
					finalColor = base * diff;
				}
			}
		}
	}

	// Calculate Luma for Alpha channel (FXAA requirement in legacy mode)
	// Using sqrt(color) to approximate gamma-space luma for better
	// detection
	float luma =
	    dot(sqrt(clamp(finalColor, 0.0, 1.0)), vec3(0.299, 0.587, 0.114));

	FragColor = vec4(finalColor, luma);
}
