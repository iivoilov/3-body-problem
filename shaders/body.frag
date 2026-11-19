#version 330 core
in vec3 worldPosition;
in vec3 worldNormal;
in vec3 localPosition;
out vec4 fragmentColor;

uniform vec3 baseColor;
uniform vec3 eye;
uniform int bodyKind; // 0 = sun, 1 = ocean planet, 2 = rocky planet
uniform float time;
uniform vec3 occluderCenter;
uniform float occluderRadius;
uniform float sunRadius;
uniform vec3 sunPosition;
uniform int shadowsEnabled;

// Fraction of the sun's apparent disk not covered by the other planet.
// Angular disk overlap gives a continuous penumbra instead of a hard point-light shadow.
float visibleSunFraction() {
    if (shadowsEnabled == 0) return 1.0;
    vec3 toSun = sunPosition - worldPosition;
    vec3 toOccluder = occluderCenter - worldPosition;
    float sunDistance = length(toSun), occluderDistance = length(toOccluder);
    if (occluderRadius <= 0.0 || sunDistance <= sunRadius ||
        occluderDistance >= sunDistance || dot(toOccluder, toSun) <= 0.0) return 1.0;
    if (occluderDistance <= occluderRadius) return 0.0;
    float a = asin(sunRadius / sunDistance);
    float b = asin(occluderRadius / occluderDistance);
    float d = acos(clamp(dot(toSun, toOccluder) / (sunDistance * occluderDistance), -1.0, 1.0));
    if (d >= a + b) return 1.0;
    if (d <= abs(a - b)) return clamp(1.0 - b*b / (a*a), 0.0, 1.0);
    float x = acos(clamp((d*d + a*a - b*b) / (2.0*d*a), -1.0, 1.0));
    float y = acos(clamp((d*d + b*b - a*a) / (2.0*d*b), -1.0, 1.0));
    float lens = sqrt(max(0.0, (-d+a+b)*(d+a-b)*(d-a+b)*(d+a+b)));
    return clamp(1.0 - (a*a*x + b*b*y - 0.5*lens) / (3.14159265*a*a), 0.0, 1.0);
}

void main() {
    vec3 n = normalize(worldNormal);
    vec3 v = normalize(eye - worldPosition);
    float pattern = sin(localPosition.x * 14.0 + sin(localPosition.z * 11.0))
                  * sin(localPosition.y * 16.0 + localPosition.z * 7.0);

    if (bodyKind == 0) {
        float activity = sin(localPosition.y * 28.0 + pattern * 2.0 + time * 0.7);
        float edge = pow(max(dot(n, v), 0.0), 0.4);
        vec3 hot = mix(vec3(1.0, 0.30, 0.035), vec3(1.0, 0.85, 0.35), edge);
        fragmentColor = vec4(hot * (0.92 + 0.08 * activity), 1.0);
        return;
    }

    vec3 surface = baseColor;
    if (bodyKind == 1) {
        float land = smoothstep(0.05, 0.24, pattern + localPosition.y * 0.15);
        surface = mix(baseColor, vec3(0.24, 0.66, 0.48), land);
        float ice = smoothstep(0.82, 0.94, abs(localPosition.y));
        surface = mix(surface, vec3(0.85, 0.95, 1.0), ice);
    } else {
        surface *= 0.78 + 0.22 * pattern;
    }

    // Use the sun's centre for light direction and its apparent size for eclipse coverage.
    vec3 light = normalize(sunPosition - worldPosition);
    float visibility = visibleSunFraction();
    float diffuse = max(dot(n, light), 0.0);
    float specular = pow(max(dot(n, normalize(light + v)), 0.0), 40.0);
    float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0);
    vec3 color = surface * (0.10 + 0.90 * diffuse * visibility)
               + vec3(1.0, 0.85, 0.65) * specular * diffuse * 0.22 * visibility
               + baseColor * rim * 0.16 * visibility;
    fragmentColor = vec4(color, 1.0);
}
