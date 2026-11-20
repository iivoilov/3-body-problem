#version 330 core
layout(location = 0) in vec3 position;
out vec2 screenUv;
void main() {
    screenUv = position.xy * 0.5 + 0.5;
    gl_Position = vec4(position.xy, 0.99999, 1.0);
}
