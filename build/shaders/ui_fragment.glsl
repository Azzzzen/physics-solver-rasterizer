#version 330 core

uniform vec3 uColorRgb;
uniform float uAlpha;
out vec4 fragColor;

void main() {
    fragColor = vec4(uColorRgb, uAlpha);
}
