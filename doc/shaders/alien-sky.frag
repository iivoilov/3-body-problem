#version 330 core
in vec2 screenUv;
out vec4 fragmentColor;
uniform vec2 viewport;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform vec3 sunDirections[3];
uniform vec3 sunColors[3];
uniform vec3 sunPowers;
uniform vec3 sunAngles;
uniform float time;
uniform float fovScale;
uniform int surfaceView;
uniform int shadowsEnabled;

float hash(vec2 p) {
    // Integer cell hashing keeps shared noise corners identical across GPU
    // optimizations; sin/dot hashing can disagree at adjacent cell boundaries.
    uvec2 cell = uvec2(ivec2(floor(p)));
    uint h = cell.x * 1597334677u ^ cell.y * 3812015801u;
    h = (h ^ (h >> 16u)) * 2246822519u;
    h = (h ^ (h >> 13u)) * 3266489917u;
    h ^= h >> 16u;
    return float(h & 0x00ffffffu) / 16777216.0;
}
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f*f*(3.0-2.0*f);
    return mix(mix(hash(i), hash(i+vec2(1,0)), f.x), mix(hash(i+vec2(0,1)), hash(i+1.0), f.x), f.y);
}
float fbm(vec2 p) {
    float value = 0.0, amplitude = 0.5;
    for (int i = 0; i < 4; ++i) {
        value += amplitude * noise(p);
        p = mat2(1.6,1.2,-1.2,1.6) * p + 13.1;
        amplitude *= 0.5;
    }
    return value;
}
float terrain(vec2 p) {
    float ridges = 1.0 - abs(2.0 * fbm(p * 0.025) - 1.0);
    float mountains = 16.0 * pow(ridges, 4.0) * smoothstep(18.0, 65.0, length(p));
    float shelf = smoothstep(0.49, 0.66, noise(p * 0.13)) * 1.1;
    return -2.5 + mountains + shelf + 0.55 * fbm(p * 0.4);
}
float terrainShadow(vec3 p, vec3 direction) {
    if (direction.y <= 0.0) return 0.0;
    if (shadowsEnabled == 0) return 1.0;
    float visibility = 1.0, distance = 0.3;
    for (int i = 0; i < 12; ++i) {
        vec3 q = p + direction * distance;
        float clearance = q.y - terrain(q.xz);
        visibility = min(visibility, 9.0 * clearance / distance);
        distance += max(0.5, distance * 0.36);
    }
    return clamp(visibility, 0.0, 1.0);
}
float refineTerrain(vec3 origin, vec3 ray, float low, float high) {
    for (int i = 0; i < 9; ++i) {
        float middle = (low+high)*0.5;
        vec3 p = origin+ray*middle;
        if (p.y > terrain(p.xz)) low = middle;
        else high = middle;
    }
    return (low+high)*0.5;
}
vec3 deepSky(vec3 ray, bool starsVisible) {
    vec2 p = vec2(atan(ray.x, ray.z), asin(clamp(ray.y,-1.0,1.0)));
    vec2 cell = floor(p * 420.0);
    vec2 local = fract(p * 420.0) - 0.5;
    float star = (1.0-smoothstep(0.0, 0.032, dot(local,local))) * step(0.992, hash(cell));
    float milkyWay = exp(-pow((ray.y+0.32*ray.x-0.18)*5.0, 2.0));
    // Diffuse texture must wrap continuously across the azimuth seam.
    float dust = fbm(ray.xz * 5.0 + ray.y * vec2(2.7,-1.3));
    vec3 color = vec3(0.006,0.010,0.019) + vec3(0.018,0.025,0.036)*milkyWay*dust;
    return color + vec3(0.52,0.61,0.75) * star * (0.3 + 0.7*hash(cell+3.0)) * (starsVisible ? 1.0 : 0.0);
}
vec3 sky(vec3 ray, bool celestialDetails) {
    float daylight = 0.0;
    for (int i = 0; i < 3; ++i)
        daylight += sqrt(min(sunPowers[i],4.0)) * smoothstep(-0.10,0.12,sunDirections[i].y);
    daylight = clamp(daylight,0.0,1.4);
    float horizon = exp(-max(ray.y,0.0)*5.0);
    vec3 color = deepSky(ray,celestialDetails) * (1.0 - min(daylight,0.96));
    color += mix(vec3(0.032,0.060,0.080), vec3(0.42,0.23,0.13), horizon) * daylight;
    float haze = fbm(ray.xz * 2.5 + vec2(time*0.04,0));
    color += vec3(0.15,0.10,0.065) * horizon * haze * daylight;
    for (int i = 0; i < 3; ++i) {
        vec3 direction = sunDirections[i];
        float cosine = clamp(dot(ray, direction),-1.0,1.0);
        float angle = acos(cosine), radius = sunAngles[i];
        float above = smoothstep(-0.03,0.015,direction.y);
        float disk = celestialDetails ? 1.0-smoothstep(radius*0.93,radius*1.04,angle) : 0.0;
        float corona = exp(-angle/max(radius*3.0,0.035)) * 0.7;
        float scattering = pow(max(cosine,0.0),18.0) * 0.25;
        vec3 warm = mix(sunColors[i],vec3(1.0,0.24,0.07),0.35*horizon);
        float coreMix = i == 0 ? 0.35 : (i == 1 ? 0.10 : 0.0);
        vec3 core = mix(warm,vec3(1.0,0.95,0.8),coreMix);
        color += above * (warm*(corona+scattering) + core*disk*3.5);
    }
    return color;
}
void main() {
    vec2 uv = screenUv*2.0-1.0;
    vec3 ray = normalize(cameraForward + fovScale * (uv.x * viewport.x/viewport.y * cameraRight + uv.y * cameraUp));
    if (surfaceView == 0) {
        fragmentColor = vec4(pow(deepSky(ray,true),vec3(0.85)),1.0);
        return;
    }
    vec3 origin = vec3(0,3.0,0);
    vec3 color = sky(ray,true);
    vec3 atmosphere = color;
    float distance = 0.2;
    bool hit = false;
    if (ray.y < 0.38) {
        float previousDistance = 0.0;
        for (int i = 0; i < 192; ++i) {
            vec3 p = origin + ray * distance;
            float clearance = p.y-terrain(p.xz);
            if (clearance < 0.0) {
                // Height-field clearance is not a signed distance. Refine the
                // first crossing rather than shading an overshot point.
                distance = refineTerrain(origin,ray,previousDistance,distance);
                hit = true;
                break;
            }
            // Grazing rays can approach a surface without crossing it. Accept
            // subpixel clearance so finite march budgets do not leave sky slits.
            float pixelFootprint = 2.0 * distance * fovScale / viewport.y;
            if (clearance < max(0.003,pixelFootprint)) { hit = true; break; }
            previousDistance = distance;
            distance += clamp(clearance * 0.35, 0.015, 3.0);
            if (distance > 320.0) break;
        }
        // A downward ray must meet the height field before its lower bound
        // y=-2.5. If grazing used the march budget, bracket that remaining hit
        // instead of returning sky from inside the landscape.
        if (!hit && ray.y < 0.0 && distance < 320.0) {
            float high = min((origin.y+2.5)/(-ray.y)+0.1,320.0);
            vec3 p = origin+ray*high;
            if (high > distance && p.y <= terrain(p.xz)) {
                distance = refineTerrain(origin,ray,previousDistance,high);
                hit = true;
            }
        }
    }
    if (hit) {
        vec3 p = origin+ray*distance;
        float e = 0.03 + distance*0.0005;
        vec3 n = normalize(vec3(terrain(p.xz-vec2(e,0))-terrain(p.xz+vec2(e,0)),2.0*e,
                                terrain(p.xz-vec2(0,e))-terrain(p.xz+vec2(0,e))));
        float pixelWidth = 2.0 * distance * fovScale / viewport.y;
        float grainDetail = 1.0-smoothstep(0.05,0.35,pixelWidth*3.0);
        float grains = mix(0.5,fbm(p.xz*3.0),grainDetail);
        // Broad, irregular layers replace the tightly repeated sine stripes.
        // Fade detail below the pixel footprint instead of letting it shimmer.
        float layerCoordinate = p.y*0.8+noise(p.xz*0.12)*1.3;
        float layerDetail = 1.0-smoothstep(0.15,0.45,pixelWidth*0.8);
        float strata = mix(0.5,noise(vec2(layerCoordinate,17.0)),layerDetail);
        float rock = 1.0-smoothstep(0.55,0.83,n.y);
        vec3 sand = mix(vec3(0.28,0.19,0.12),vec3(0.49,0.35,0.21),grains);
        vec3 basalt = mix(vec3(0.055,0.058,0.060),vec3(0.085,0.078,0.067),strata);
        vec3 albedo = mix(sand,basalt,rock);
        vec3 light = vec3(0.012,0.020,0.027);
        vec3 specular = vec3(0);
        for (int i = 0; i < 3; ++i) {
            float visibility = terrainShadow(p+n*0.15, sunDirections[i]);
            float power = min(sqrt(sunPowers[i]),2.5);
            float diffuse = max(dot(n,sunDirections[i]),0.0);
            light += sunColors[i] * power * diffuse * visibility * 1.8;
            vec3 halfway = normalize(sunDirections[i]-ray);
            specular += sunColors[i]*pow(max(dot(n,halfway),0.0),55.0)*visibility*power*0.10;
        }
        color = albedo * light + specular;
        float fog = 1.0-exp(-distance*0.007);
        // Fog scatters broad sky light, not sharp stars/disks stretched down
        // a terrain column by this horizon lookup.
        color = mix(color,sky(normalize(vec3(ray.x,0.045,ray.z)),false)*0.68,fog);
        color = mix(color,atmosphere,smoothstep(240.0,320.0,distance));
    }
    color *= 1.15;
    color = color / (color+vec3(0.8));
    color = pow(max(color,vec3(0)),vec3(0.4545));
    float vignette = 1.0 - 0.16*dot(uv*vec2(0.65,1.0),uv*vec2(0.65,1.0));
    color *= vignette;
    color += (hash(gl_FragCoord.xy+floor(time*12.0))-0.5)/255.0;
    fragmentColor = vec4(color,1.0);
}
