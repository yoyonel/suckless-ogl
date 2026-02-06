#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;  // L'atlas de la font
uniform vec3 textColor;  // La couleur passée depuis le C

uniform int useTexture;  // 1 = Text (Atlas), 0 = Solid Rect

uniform float globalAlpha;  // Transparence globale (0.0 - 1.0)

uniform vec2 rectSize;  // Dimensions en pixels
uniform float radius;   // Rayon des coins

void main()
{
	float finalAlpha = globalAlpha;

	if (useTexture == 1) {
		/* Font Atlas */
		finalAlpha *= texture(text, TexCoords).r;
	} else if (useTexture == 2) {
		/* Rounded Rect (SDF) */
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

	color = vec4(textColor, finalAlpha);

	if (color.a < 0.001)
		discard;
}
