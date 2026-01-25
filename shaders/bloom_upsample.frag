#version 330 core

/*
 * Upsampling avec Tent Filter (3x3)
 * Rayon ajustable par le scale, mais standard est 1.0 (voisins immédiats).
 */

in vec2 TexCoords;
out vec3 FragColor;

uniform sampler2D srcTexture;
uniform float filterRadius; /* Rayon du filtre, défaut 1.0 */

void main()
{
	// La taille du filtre dépend de la résolution de la texture source
	ivec2 sz = textureSize(srcTexture, 0);
	float x = filterRadius / float(sz.x);
	float y = filterRadius / float(sz.y);

	// 9-tap tent filter
	// [ 1 2 1 ]
	// [ 2 4 2 ]
	// [ 1 2 1 ] / 16
	vec3 a =
	    texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
	vec3 b = texture(srcTexture, vec2(TexCoords.x, TexCoords.y + y)).rgb;
	vec3 c =
	    texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;

	vec3 d = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y)).rgb;
	vec3 e = texture(srcTexture, vec2(TexCoords.x, TexCoords.y)).rgb;
	vec3 f = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y)).rgb;

	vec3 g =
	    texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
	vec3 h = texture(srcTexture, vec2(TexCoords.x, TexCoords.y - y)).rgb;
	vec3 i =
	    texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

	FragColor = (e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i)) / 16.0;
}
