#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;     // L'atlas de la font
uniform vec3 textColor;     // La couleur passée depuis le C

uniform int useTexture;     // 1 = Text (Atlas), 0 = Solid Rect

void main() {
    float alpha = 1.0;
    
    if (useTexture != 0) {
        alpha = texture(text, TexCoords).r;
         if (alpha < 0.1) discard;
    }

    color = vec4(textColor, alpha);
}