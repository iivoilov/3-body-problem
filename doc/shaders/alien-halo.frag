#version 330 core
in vec2 uv;
out vec4 fragmentColor;
uniform vec3 color;
void main() {
    float r = length(uv);
    float glow = exp(-r*r*10.0)*(1.0-smoothstep(0.6,1.0,r));
    fragmentColor = vec4(color,glow*0.5);
}
