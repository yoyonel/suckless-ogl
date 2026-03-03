#version 330 core
in vec2 TexCoords;
in vec4 vColor;
in float vMode;
in vec3 vRoundedParams;

out vec4 color;

uniform sampler2D text;  // L'atlas de la font

void main()
{
	float finalAlpha = vColor.a;

	// vMode: 0.0 = Solid Rect, 1.0 = Text, 2.0 = Rounded Rect
	if (vMode > 0.5 && vMode < 1.5) {
		/* Font Atlas */
		finalAlpha *= texture(text, TexCoords).r;
	} else if (vMode > 1.5) {
		/* Rounded Rect (SDF) */
		vec2 rectSize = vRoundedParams.xy;
		float radius = vRoundedParams.z;
		// Coordonnées locales en pixels, centrées
		vec2 p = (TexCoords * rectSize) - (rectSize * 0.5);
		vec2 b = (rectSize * 0.5) - vec2(radius);

		// Distance au rectangle arrondi
		vec2 d = abs(p) - b;
		float dist =
		    length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radius;

		// Anti-aliasing (1.0 pixel soft edge)
		float alpha = 1.0 - smoothstep(-0.5, 0.5, dist);
		finalAlpha *= alpha;
	}

	color = vec4(vColor.rgb, finalAlpha);

	if (color.a < 0.001)
		discard;
}
