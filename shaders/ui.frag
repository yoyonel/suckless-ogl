#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;     // L'atlas de la font
uniform vec3 textColor;     // La couleur passée depuis le C

void main() {
    // On récupère la valeur de gris dans le canal Rouge
    float alpha = texture(text, TexCoords).r;
    
    // Si l'alpha est trop bas, on ne dessine rien (évite les artefacts)
    if (alpha < 0.1) discard;

    color = vec4(textColor, alpha);
}