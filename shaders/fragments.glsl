#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D faceTexture;

void main() {
    FragColor = texture(faceTexture, TexCoord);
}
