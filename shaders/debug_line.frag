#version 330 core
out vec4 FragColor;

in vec3 vWorldPos;
in vec3 vAlbedo;

uniform vec4 u_color;
uniform bool u_stippled;
uniform bool u_useInstanceColor;

void main()
{
	if (u_stippled) {
		// World-space stippling to avoid jitter
		// We use a simple pattern based on world coordinates
		// Summing x+y+z gives distinct diagonal bands
		float pattern = vWorldPos.x + vWorldPos.y + vWorldPos.z;

		// Scale the pattern to control frequency (higher = denser dots)
		// 50.0 puts dots roughly every 2cm
		if (sin(pattern * 50.0) < 0.0) {
			discard;
		}
	}

	if (u_useInstanceColor) {
		// Explicitly construct vec4 with u_color.a
		FragColor = vec4(vAlbedo, u_color.a);
	} else {
		FragColor = u_color;
	}
}
