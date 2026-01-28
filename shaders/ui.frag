#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;  // L'atlas de la font
uniform vec3 textColor;  // La couleur passée depuis le C

uniform int useTexture;  // 1 = Text (Atlas), 0 = Solid Rect

void main()
{
	if (useTexture != 0) {
		/* Font Atlas or Image */
		float a = texture(text, TexCoords).r;
		/* Simple alpha text */
		color = vec4(textColor, a);
	} else {
		/* Solid Rect */
		color = vec4(textColor, 1.0);
	}

	if (color.a < 0.1)
		discard;
}
