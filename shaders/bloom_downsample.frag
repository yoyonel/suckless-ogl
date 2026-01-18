#version 330 core

/* 
 * Downsampling 13-tap de Jorge Jimenez (Next Gen Post Processing in Call of Duty: Advanced Warfare)
 * Mieux que le simple bilinear 4-tap, réduit le flickering et garde l'énergie.
 */

in vec2 TexCoords;
out vec3 FragColor;

uniform sampler2D srcTexture;
uniform vec2 srcResolution; /* Résolution de la texture source (pas la destination !) */

void main() {
    vec2 srcTexelSize = 1.0 / srcResolution;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    /* On échantillonne autour du centre */
    vec3 a = texture(srcTexture, vec2(TexCoords.x - 2*x, TexCoords.y + 2*y)).rgb;
    vec3 b = texture(srcTexture, vec2(TexCoords.x,       TexCoords.y + 2*y)).rgb;
    vec3 c = texture(srcTexture, vec2(TexCoords.x + 2*x, TexCoords.y + 2*y)).rgb;

    vec3 d = texture(srcTexture, vec2(TexCoords.x - 2*x, TexCoords.y)).rgb;
    vec3 e = texture(srcTexture, vec2(TexCoords.x,       TexCoords.y)).rgb;
    vec3 f = texture(srcTexture, vec2(TexCoords.x + 2*x, TexCoords.y)).rgb;

    vec3 g = texture(srcTexture, vec2(TexCoords.x - 2*x, TexCoords.y - 2*y)).rgb;
    vec3 h = texture(srcTexture, vec2(TexCoords.x,       TexCoords.y - 2*y)).rgb;
    vec3 i = texture(srcTexture, vec2(TexCoords.x + 2*x, TexCoords.y - 2*y)).rgb;

    vec3 j = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
    vec3 k = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;
    vec3 l = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
    vec3 m = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

    /* Pondération pour garder l'énergie stable */
    FragColor = e*0.125;
    FragColor += (a+c+g+i)*0.03125;
    FragColor += (b+d+f+h)*0.0625;
    FragColor += (j+k+l+m)*0.125;
}
