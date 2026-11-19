#version 330 core
layout(location = 0) in vec3 position;
uniform mat4 viewProjection;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform vec3 sunPosition;
out vec2 uv;
void main() {
    uv = position.xy;
    vec3 world = sunPosition + (cameraRight * position.x + cameraUp * position.y) * 3.3;
    gl_Position = viewProjection * vec4(world, 1.0);
}
