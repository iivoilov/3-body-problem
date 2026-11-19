// Adapted from the native OpenGL vertex shader for Three.js ShaderMaterial (GLSL ES 3).
out vec3 worldPosition;
out vec3 worldNormal;
out vec3 localPosition;
void main() {
    vec4 world = modelMatrix * vec4(position, 1.0);
    worldPosition = world.xyz;
    worldNormal = normalize(mat3(modelMatrix) * normal);
    localPosition = position;
    gl_Position = projectionMatrix * viewMatrix * world;
}
