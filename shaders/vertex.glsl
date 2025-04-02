precision mediump float;
uniform sampler2D uTexture;
varying vec2 vTexCoord;

void main() {
    vec4 texColor = texture2D(uTexture, vTexCoord);
    gl_FragColor = texColor; // Assume texture has alpha
}
