#pragma once

#include "physics.hpp"
#include <cstdint>
#include <deque>

namespace orbit {
inline constexpr double periapsisEccentricityFloor = 1e-4;
inline double wrapAngle(double angle) { return std::remainder(angle, 2 * physicsPi); }

struct OrbitalElements {
    double eccentricity = 0, semimajorAxis = 0, period = 0;
    double meanLongitude = 0, periapsis = 0;
    bool bound = false, planarPrograde = false, phaseReliable = false;
};

// Instantaneous sun-relative two-body (osculating) elements, not a fit to the trail.
// Longitudes use our prograde XZ plane: x = r cos(longitude), z = r sin(longitude).
inline OrbitalElements orbitalElements(const Body& sun, const Body& planet) {
    OrbitalElements result;
    const Vec3d r = planet.position - sun.position, v = planet.velocity - sun.velocity;
    const double distance = length(r), mu = gravity * (sun.mass + planet.mass);
    if (distance <= 0 || mu <= 0) return result;
    const Vec3d h = cross(r, v);
    result.planarPrograde = h.y < 0 && std::hypot(h.x, h.z) <= length(h) * 1e-8;
    const Vec3d eccentricityVector = cross(v, h) * (1 / mu) - r * (1 / distance);
    result.eccentricity = length(eccentricityVector);
    const double specificEnergy = dot(v, v) / 2 - mu / distance;
    if (specificEnergy >= 0 || result.eccentricity >= 1) return result;
    result.bound = true;
    result.semimajorAxis = -mu / (2 * specificEnergy);
    result.period = 2 * physicsPi * std::sqrt(std::pow(result.semimajorAxis, 3) / mu);
    result.periapsis = std::atan2(eccentricityVector.z, eccentricityVector.x);
    if (result.eccentricity > 1e-10) {
        const double e = result.eccentricity, a = result.semimajorAxis;
        const double cosE = (1 - distance / a) / e;
        const double sinE = dot(r, v) / (e * std::sqrt(mu * a));
        const double eccentricAnomaly = std::atan2(sinE, cosE);
        result.meanLongitude = wrapAngle(result.periapsis + eccentricAnomaly - e * std::sin(eccentricAnomaly));
    } else {
        result.meanLongitude = std::atan2(r.z, r.x);
    }
    // The phase formula below is for planar prograde orbits; periapsis is ill-defined at e = 0.
    result.phaseReliable = result.eccentricity >= periapsisEccentricityFloor && result.planarPrograde;
    return result;
}

struct OrbitalSample {
    double time = 0, periodRatio = 0;
    std::array<OrbitalElements, 2> elements;
    std::array<double, 2> phaseTurns{};
    std::array<bool, 2> phaseValid{};
    std::array<std::uint64_t, 2> phaseSegment{};
    bool ratioValid = false;
};

// Each phase is tracked only over contiguous reliable samples. A gap starts a new segment.
class PhaseTracker {
public:
    double update(double angle, bool reliable) {
        if (!reliable) { hasPrevious_ = false; return 0; }
        const double wrapped = wrapAngle(angle);
        if (!hasPrevious_) { unwrapped_ = wrapped; ++segment_; }
        else unwrapped_ += wrapAngle(wrapped - previous_);
        hasPrevious_ = true;
        previous_ = wrapped;
        return unwrapped_ / (2 * physicsPi);
    }
    std::uint64_t segment() const { return segment_; }
private:
    bool hasPrevious_ = false;
    double previous_ = 0, unwrapped_ = 0;
    std::uint64_t segment_ = 0;
};

class OrbitDiagnostics {
public:
    static constexpr std::uint64_t stride = 128; // 0.5 simulated second at the fixed physics step.
    static constexpr std::size_t capacity = 2049; // Last 1024 simulated seconds, including endpoints.
    std::deque<OrbitalSample> history;

    void record(const Bodies& bodies, double time) {
        OrbitalSample sample;
        sample.time = time;
        for (std::size_t i = 0; i < 2; ++i) sample.elements[i] = orbitalElements(bodies[0], bodies[i+1]);
        sample.ratioValid = sample.elements[0].bound && sample.elements[1].bound;
        if (sample.ratioValid) sample.periodRatio = sample.elements[1].period / sample.elements[0].period;
        for (std::size_t i = 0; i < 2; ++i) {
            sample.phaseValid[i] = sample.ratioValid && sample.elements[0].planarPrograde &&
                                   sample.elements[1].planarPrograde && sample.elements[i].phaseReliable;
            const double angle = 2 * sample.elements[1].meanLongitude - sample.elements[0].meanLongitude
                               - sample.elements[i].periapsis;
            sample.phaseTurns[i] = phases_[i].update(angle, sample.phaseValid[i]);
            sample.phaseSegment[i] = phases_[i].segment();
        }
        history.push_back(sample);
        if (history.size() > capacity) history.pop_front();
    }
private:
    std::array<PhaseTracker, 2> phases_;
};
} // namespace orbit
