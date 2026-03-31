#version 450 core

layout(location = 0) in vec4 vertex;  // <vec2 pos, vec2 tex>

layout(location = 0) out vec2 TexCoords;

layout(location = 0) uniform mat4 projection;
layout(location = 4) uniform mat4 model;

void main()
{
	// vertex.xy contains local quad coordinates (e.g. -0.5 to 0.5)
	// model matrix handles Scale -> Rotate -> Translate
	gl_Position = projection * model * vec4(vertex.xy, 0.0, 1.0);
	TexCoords = vertex.zw;
}
