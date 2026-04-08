#version 440 core
layout(location = 0) out float FragColor;
layout(location = 0) in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneTexture;

void main()
{
	vec2 texSize = textureSize(sceneTexture, 0);
	vec2 onePixel = 1.0 / texSize;

	/* Super-sampling simple (4x4 box filter) pour capturer les pics de
	   lumière et stabiliser la moyenne (evite le flickering temporel quand
	   on bouge) */
	float totalLogLum = 0.0;
	float weight = 0.0;

	/* On couvre une zone correspondant au ratio de réduction */
	/* Ex: 1024 -> 64 = ratio 16. On sample un bloc 4x4 avec un stride de 4
	 * pixels */
	/* C'est une approximation bon marché d'un mipmap generation */

	for (float y = -1.5; y <= 1.5; y += 1.0) {
		for (float x = -1.5; x <= 1.5; x += 1.0) {
			vec2 offset = vec2(x, y) * 4.0; /* Stride 4 pixels */
			vec3 color =
			    texture(sceneTexture, TexCoords + offset * onePixel)
			        .rgb;

			float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));

			/* Masquage des pixels trop sombres (Fond noir, coins
			   sombres) pour éviter qu'ils ne faussent la moyenne
			   géométrique vers le bas */
			if (lum > 0.05 && !isinf(lum)) {
				totalLogLum += log2(lum);
				weight += 1.0;
			}
		}
	}

	if (weight > 0.0) {
		FragColor = totalLogLum / weight;
	} else {
		/* Valeur SENTINELLE pour dire "Ignorer ce bloc" au Compute
		 * Shader */
		FragColor = -100.0;
	}
}
