#version 450 core

layout(location = 0) in vec2 TexCoords;
layout(location = 0) out vec4 FragColor;

layout(location = 8) uniform vec3 color;

void main()
{
	vec2 uv = TexCoords - 0.5;
	float dist = length(uv);
	float angle = atan(uv.y, uv.x);  // -PI to PI

	// Ring configuration
	float radius = 0.4;
	float thickness = 0.08;

	// Smooth edges (AA)
	float delta = fwidth(dist);
	float alphaRing =
	    smoothstep(radius - thickness - delta, radius - thickness, dist) *
	    (1.0 -
	     smoothstep(radius + thickness, radius + thickness + delta, dist));

	// Gradient Tail
	// Normalizing angle from [-PI, PI] to [0, 1]
	float gradient = (angle + 3.14159265) / 6.2831853;

	// Final Alpha
	float alpha = alphaRing * gradient;

	if (alpha < 0.01)
		discard;

	FragColor = vec4(color, alpha);
}
