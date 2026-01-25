#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D tex;
uniform float lod;

void main()
{
	// On utilise textureLod pour forcer l'affichage d'un étage spécifique
	FragColor = vec4(textureLod(tex, TexCoords, lod).rgb, 1.0);

	// Tonemapping simple pour le debug (Reinhard)
	FragColor.rgb = FragColor.rgb / (FragColor.rgb + vec3(1.0));
	// Gamma correction
	FragColor.rgb = pow(FragColor.rgb, vec3(1.0 / 2.2));
}
