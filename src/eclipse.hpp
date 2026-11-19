#pragma once

#include "scene.hpp"
#include <utility>

namespace orbit {
// Approximate the apparent sun and occluder as overlapping angular disks.
// This CPU version supplies the HUD and geometry tests; body.frag uses the same model per fragment.
inline float visibleSunFraction(Vec3 point, Vec3 occluder, float occluderRadius, Vec3 sunPosition = {}) {
    const Vec3 toSun = sunPosition - point, toOccluder = occluder - point;
    const float sunDistance = length(toSun), occluderDistance = length(toOccluder);
    if (occluderRadius <= 0 || sunDistance <= sunRadius ||
        occluderDistance >= sunDistance || dot(toOccluder, toSun) <= 0) return 1;
    if (occluderDistance <= occluderRadius) return 0;
    const float a = std::asin(sunRadius / sunDistance);
    const float b = std::asin(occluderRadius / occluderDistance);
    const float d = std::acos(std::clamp(dot(toSun, toOccluder) / (sunDistance * occluderDistance), -1.0f, 1.0f));
    if (d >= a + b) return 1;
    if (d <= std::abs(a - b)) return std::clamp(1 - (b * b) / (a * a), 0.0f, 1.0f);
    const float x = std::acos(std::clamp((d*d + a*a - b*b) / (2*d*a), -1.0f, 1.0f));
    const float y = std::acos(std::clamp((d*d + b*b - a*a) / (2*d*b), -1.0f, 1.0f));
    const float lens = std::sqrt(std::max(0.0f, (-d+a+b)*(d+a-b)*(d-a+b)*(d+a+b)));
    return std::clamp(1 - (a*a*x + b*b*y - 0.5f*lens) / (pi*a*a), 0.0f, 1.0f);
}

// Search forward by integrating a copy, rather than imposing an analytic orbit or teleporting bodies.
// Leave the caller unchanged if there is no conjunction in four initial outer periods.
inline bool seekNextEclipse(Simulation& simulation) {
    Simulation candidate = simulation;
    const auto alignment = [](const Bodies& bodies) {
        const Vec3d a = bodies[1].position - bodies[0].position;
        const Vec3d b = bodies[2].position - bodies[0].position;
        return a.x * b.z - a.z * b.x;
    };
    double previous = alignment(candidate.bodies);
    const int limit = static_cast<int>(4 * initialPeriod(planets[1]) / physicsStep);
    for (int i = 0; i < limit; ++i) {
        if (!candidate.step()) return false;
        const double current = alignment(candidate.bodies);
        if (((previous > 0 && current <= 0) || (previous < 0 && current >= 0)) &&
            dot(candidate.bodies[1].position - candidate.bodies[0].position,
                candidate.bodies[2].position - candidate.bodies[0].position) > 0) {
            candidate.paused = true;
            simulation = std::move(candidate);
            return true;
        }
        previous = current;
    }
    return false;
}
} // namespace orbit
