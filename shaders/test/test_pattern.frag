#version 450 core

out vec4 FragColor;

in vec2 TexCoords;

uniform float u_time;
uniform vec2 u_resolution;

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

	// Split screen: Star on left, Grid on right
	float color = 0.0;
	if (uv.x < 0.5) {
		// Left: Siemens Star
		vec2 starUV = vec2(uv.x * 2.0, uv.y);
		color = siemensStar(starUV, 32.0);
	} else {
		// Right: Rotated Grid (aliasing-prone angle like 15 degrees)
		vec2 gridUV = vec2((uv.x - 0.5) * 2.0, uv.y);
		color = rotatedGrid(gridUV, 15.0 * PI / 180.0, 0.02);
	}

	// High contrast Black & White
	FragColor = vec4(vec3(color), 1.0);
}
