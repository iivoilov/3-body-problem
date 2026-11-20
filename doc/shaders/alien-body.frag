#version 330 core
in vec3 worldPosition;
in vec3 worldNormal;
in vec3 localPosition;
out vec4 fragmentColor;
uniform vec3 eye;
uniform vec3 baseColor;
uniform vec3 starPositions[3];
uniform vec3 starColors[3];
uniform float time;
uniform int starBody;
void main() {
    vec3 n = normalize(worldNormal), view = normalize(eye-worldPosition);
    float pattern = sin(localPosition.x*23.0+sin(localPosition.z*19.0+time*2.0))
                  * sin(localPosition.y*25.0+localPosition.z*11.0);
    if (starBody == 1) {
        float facing = pow(max(dot(n,view),0.0),0.3);
        vec3 color = mix(baseColor*0.7,mix(baseColor,vec3(1.0,0.98,0.88),0.72),facing);
        color *= 0.92+0.08*pattern;
        fragmentColor = vec4(color,1.0);
        return;
    }
    float continents = smoothstep(-0.2,0.5,pattern);
    vec3 surface = mix(vec3(0.13,0.19,0.22),vec3(0.49,0.29,0.15),continents);
    float cracks = pow(1.0-abs(sin(localPosition.y*34.0+pattern*2.0)),20.0);
    surface *= 1.0-cracks*0.32;
    vec3 illumination = vec3(0.035,0.045,0.065);
    for (int i = 0; i < 3; ++i) {
        vec3 light = normalize(starPositions[i]-worldPosition);
        illumination += starColors[i]*max(dot(n,light),0.0)*(i==2 ? 0.10 : 0.8);
    }
    float rim = pow(1.0-max(dot(n,view),0.0),3.0);
    fragmentColor = vec4(surface*illumination+vec3(0.21,0.48,0.55)*rim*0.45,1.0);
}
