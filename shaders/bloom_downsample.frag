#version 450 core

/**
 * Downsample shader (13-tap filter)
 * Used by Bloom (legacy) and DoF.
 */

in vec2 TexCoords;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D srcTexture;
uniform vec2 srcResolution;

void main()
{
	vec2 texelSize = 1.0 / srcResolution;
	float x = texelSize.x;
	float y = texelSize.y;

	// Take 13 samples around current texel:
	// a - b - c
	// - j - k -
	// d - e - f
	// - l - m -
	// g - h - i
	// === 13 samples ===

	vec3 a =
	    texture(srcTexture, vec2(TexCoords.x - 2 * x, TexCoords.y + 2 * y))
	        .rgb;
	vec3 b =
	    texture(srcTexture, vec2(TexCoords.x, TexCoords.y + 2 * y)).rgb;
	vec3 c =
	    texture(srcTexture, vec2(TexCoords.x + 2 * x, TexCoords.y + 2 * y))
	        .rgb;

	vec3 d =
	    texture(srcTexture, vec2(TexCoords.x - 2 * x, TexCoords.y)).rgb;
	vec3 e = texture(srcTexture, vec2(TexCoords.x, TexCoords.y)).rgb;
	vec3 f =
	    texture(srcTexture, vec2(TexCoords.x + 2 * x, TexCoords.y)).rgb;

	vec3 g =
	    texture(srcTexture, vec2(TexCoords.x - 2 * x, TexCoords.y - 2 * y))
	        .rgb;
	vec3 h =
	    texture(srcTexture, vec2(TexCoords.x, TexCoords.y - 2 * y)).rgb;
	vec3 i =
	    texture(srcTexture, vec2(TexCoords.x + 2 * x, TexCoords.y - 2 * y))
	        .rgb;

	vec3 j =
	    texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
	vec3 k =
	    texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;
	vec3 l =
	    texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
	vec3 m =
	    texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

	// Apply weights (Karis 2013)
	vec3 result = e * 0.125;
	result += (a + c + g + i) * 0.03125;
	result += (b + d + f + h) * 0.0625;
	result += (j + k + l + m) * 0.125;

	FragColor = vec4(max(result, vec3(0.0001)), 1.0);
}
