#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 model;
uniform mat4 viewProjection;
out vec3 worldPosition;
out vec3 worldNormal;
out vec3 localPosition;

void main() {
    worldPosition = vec3(model * vec4(position, 1.0));
    // Our bodies only use uniform scale. Non-uniform scale would need an inverse transpose.
    worldNormal = normalize(mat3(model) * normal);
    localPosition = position;
    gl_Position = viewProjection * vec4(worldPosition, 1.0);
}
