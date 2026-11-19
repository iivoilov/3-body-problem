#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
uniform vec2 viewport;
out vec3 vertexColor;
void main() {
    vec2 ndc = position / viewport * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    vertexColor = color;
}
