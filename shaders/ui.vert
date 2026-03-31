#version 450 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aMode;
layout(location = 4) in vec3 aRoundedParams;  // width, height, radius

layout(location = 0) out vec2 TexCoords;
layout(location = 1) out vec4 vColor;
layout(location = 2) out float vMode;
layout(location = 3) out vec3 vRoundedParams;

layout(location = 0) uniform mat4 projection;

void main()
{
	gl_Position = projection * vec4(aPos, 0.0, 1.0);
	TexCoords = aTexCoords;
	vColor = aColor;
	vMode = aMode;
	vRoundedParams = aRoundedParams;
}
