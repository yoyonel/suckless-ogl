// ============= shaders/phong.frag =============
#version 330 core

in vec3 fragNormal;
in vec3 fragPos;

out vec4 color;

uniform vec3 lightDir;

void main()
{
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightDir);

    /* Diffuse lighting */
    float diff = max(dot(N, L), 0.0);

    /* Specular lighting */
    vec3 V = normalize(-fragPos);
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), 32.0);

    /* Combine lighting */
    vec3 ambient = 0.1 * vec3(0.4, 0.7, 1.0);
    vec3 diffuse = diff * vec3(0.4, 0.7, 1.0);
    vec3 specular = spec * vec3(1.0);

    color = vec4(ambient + diffuse + specular, 1.0);
}
