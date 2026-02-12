#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D u_tex;
uniform float lod;
uniform float u_alpha = 1.0;
uniform bool u_bypass_processing = false;

void main()
{
	vec4 texColor = textureLod(u_tex, TexCoords, lod);
	FragColor = vec4(texColor.rgb, u_alpha);

	if (!u_bypass_processing) {
		// Tonemapping simple pour le debug (Reinhard)
		FragColor.rgb = FragColor.rgb / (FragColor.rgb + vec3(1.0));
		// Gamma correction
		FragColor.rgb = pow(FragColor.rgb, vec3(1.0 / 2.2));
	}
}
