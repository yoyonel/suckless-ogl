#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;  // L'atlas de la font
uniform vec3 textColor;  // La couleur passée depuis le C

uniform int useTexture;  // 1 = Text (Atlas), 0 = Solid Rect

uniform float globalAlpha;  // Transparence globale (0.0 - 1.0)

void main()
{
	float finalAlpha = globalAlpha;
	if (useTexture != 0) {
		/* Font Atlas or Image */
		finalAlpha *= texture(text, TexCoords).r;
	}

	color = vec4(textColor, finalAlpha);

	if (color.a < 0.001)
		discard;
}
