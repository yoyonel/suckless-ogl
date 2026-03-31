#version 450 core
layout(location = 0) in vec2 TexCoords;
layout(location = 1) in vec4 vColor;
layout(location = 2) in float vMode;
layout(location = 3) in vec3 vRoundedParams;

layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D
    text;  // Font atlas (modes 0-2) ou texture PNG (modes 3-4)

void main()
{
	// vMode: 0.0 = Solid Rect, 1.0 = Text, 2.0 = Rounded Rect
	//        3.0 = Textured Tinted, 4.0 = Textured Additive (Bloom)

	if (vMode > 2.5 && vMode < 3.5) {
		/* Mode 3: Textured quad teinté.
		 * Sample RGBA depuis la texture PNG, teinte par vColor.rgb,
		 * alpha final = tex.a * vColor.a
		 */
		vec4 tex = texture(text, TexCoords);
		color = vec4(tex.rgb * vColor.rgb, tex.a * vColor.a);
		if (color.a < 0.002)
			discard;
		return;
	}

	if (vMode > 3.5) {
		/* Mode 4: Textured additive (Bloom glow).
		 * La texture est un bloom radial blanc sur fond noir.
		 * La luminance du pixel sert d'alpha pour l'effet additif —
		 * le rendu se fait avec glBlendFunc(GL_ONE, GL_ONE) côté C.
		 * vColor.a porte l'intensité globale du bloom
		 * (help_press_timer).
		 */
		vec4 tex = texture(text, TexCoords);
		float lum = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
		color = vec4(tex.rgb * vColor.rgb, lum * vColor.a);
		if (color.a < 0.002)
			discard;
		return;
	}

	float finalAlpha = vColor.a;

	// vMode: 0.0 = Solid Rect, 1.0 = Text, 2.0 = Rounded Rect
	if (vMode > 0.5 && vMode < 1.5) {
		/* Font Atlas */
		finalAlpha *= texture(text, TexCoords).r;
	} else if (vMode > 1.5 && vMode < 2.5) {
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
	} else if (vMode > 4.5 && vMode < 5.5) {
		/* SDF Neon Glow Border (Mode 5.0)
		 * Dessine un contour lumineux qui s'estompe vers l'extérieur et
		 * l'intérieur. Idéal pour l'effet de touche de clavier pressée.
		 */
		vec2 rectSize = vRoundedParams.xy;
		float radius = vRoundedParams.z;
		vec2 p = (TexCoords * rectSize) - (rectSize * 0.5);
		vec2 b = (rectSize * 0.5) - vec2(radius);

		vec2 d = abs(p) - b;
		float dist =
		    length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radius;

		// Le "glow" est maximal sur la bordure (dist = 0) et s'efface
		// vite Falloff : plus le diviseur est grand, plus le halo est
		// large.
		float glow = exp(-abs(dist) / 3.0);

		// On adoucit l'intérieur pour garder l'effet de bordure pure
		float intensity = glow * (1.0 - smoothstep(1.0, 3.0, -dist));

		finalAlpha *= intensity;
	}

	color = vec4(vColor.rgb, finalAlpha);

	if (color.a < 0.001)
		discard;
}
