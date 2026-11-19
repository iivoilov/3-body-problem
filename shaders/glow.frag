#version 330 core
in vec2 uv;
out vec4 fragmentColor;
void main() {
    float r = length(uv);
    float alpha = exp(-r * r * 6.0) * (1.0 - smoothstep(0.75, 1.0, r));
    fragmentColor = vec4(1.0, 0.40, 0.08, alpha * 0.48);
}
