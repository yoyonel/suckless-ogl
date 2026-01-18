#version 330 core

in vec2 TexCoords;
out vec3 FragColor;

uniform sampler2D srcTexture;
uniform float threshold;
uniform float knee; /* Soft threshold knee */

void main() {
    vec3 color = texture(srcTexture, TexCoords).rgb;
    
    /* Calcul de la luminance (perception humaine) */
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    /* Courbe de seuil progressif (Quadratic threshold curve de UE4) */
    /* knee est "soft_threshold" dans nos params */
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    
    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 0.00001);
    
    FragColor = color * contribution;
}
