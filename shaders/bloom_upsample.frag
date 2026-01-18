#version 330 core

/*
 * Upsampling avec Tent Filter (3x3)
 * Rayon ajustable par le scale, mais standard est 1.0 (voisins immédiats).
 */

in vec2 TexCoords;
out vec3 FragColor;

uniform sampler2D srcTexture;
uniform float filterRadius; /* Rayon du filtre, défaut 1.0 */

void main() {
    /* La taille du filtre dépend de la résolution texture SOURCE (techniquement on peut utiliser gl_FragCoord ou uniform) */
    int w, h;
    ivec2 sz = textureSize(srcTexture, 0);
    float x = filterRadius / float(sz.x);
    float y = filterRadius / float(sz.y);

    /* 9-tap tent filter pattern */
    vec3 d = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
    vec3 e = texture(srcTexture, vec2(TexCoords.x,     TexCoords.y + y)).rgb;
    vec3 f = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;

    vec3 g = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y)).rgb;
    vec3 h_center = texture(srcTexture, vec2(TexCoords.x,     TexCoords.y)).rgb;
    vec3 i = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y)).rgb;

    vec3 j = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
    vec3 k = texture(srcTexture, vec2(TexCoords.x,     TexCoords.y - y)).rgb;
    vec3 l = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

    FragColor = e*0.0625 + (d+f+g+i)*0.03125 + h_center*0.25 + (j+l)*0.03125 + k*0.0625;
    
    /* Note: pour le tent filter 3x3 simple :
       1 2 1
       2 4 2
       1 2 1
       / 16
       
       Ceci est une approximation, le standard Dual Filter est plus complexe.
       Pour l'instant, une simple bilinear interpolation (hardware) suffirait si on ne voulait pas de radius custom,
       mais le Tent filter donne un rendu plus "smooth".
    */
    
    FragColor = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb * 0.25 +
                texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb * 0.25 +
                texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb * 0.25 +
                texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb * 0.25;
                
    /* Attends, le code ci-dessus est un simple box 4-tap, pas un tent. 
       Reprenons le vrai Tent Filter (9 samples) avec poids.
    */
    
    vec4 sum = vec4(0.0);
    sum += texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y + y));
    sum += texture(srcTexture, vec2(TexCoords.x + 0, TexCoords.y + y)) * 2.0;
    sum += texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y + y));
    
    sum += texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y + 0)) * 2.0;
    sum += texture(srcTexture, vec2(TexCoords.x + 0, TexCoords.y + 0)) * 4.0;
    sum += texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y + 0)) * 2.0;
    
    sum += texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y - y));
    sum += texture(srcTexture, vec2(TexCoords.x + 0, TexCoords.y - y)) * 2.0;
    sum += texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y - y));
    
    FragColor = sum.rgb / 16.0;
}
