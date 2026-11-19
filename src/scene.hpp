#pragma once

#include "physics.hpp"
#include "orbit_diagnostics.hpp"
#include <array>
#include <cstdint>
#include <deque>

namespace orbit {
struct Planet {
    const char* name;
    float radius;
    float orbitRadius; // Initial semimajor axis; also the initial distance in the circular preset.
    double mass;
    double initialAngle;
    Vec3 color;
};

inline constexpr double sunMass = 1000;
inline constexpr double initialPeriodRatio = 2.05; // Ember / Azure, using initial two-body periods.
inline const std::array<Planet, 2> planets = [] {
    std::array<Planet, 2> result{{
        {"AZURE", 0.65f, 5.0f, 2.0, 0.35, {0.18f, 0.65f, 0.95f}},
        {"EMBER", 0.85f, 0, 4.0, 2.65, {0.95f, 0.38f, 0.20f}}
    }};
    // T^2 = 4*pi^2*r^3 / G(M+m), including the different planetary masses.
    result[1].orbitRadius = static_cast<float>(result[0].orbitRadius *
        std::cbrt(initialPeriodRatio * initialPeriodRatio * (sunMass+result[1].mass) / (sunMass+result[0].mass)));
    return result;
}();
inline constexpr float sunRadius = 1.25f;
inline const std::array<float, 3> bodyRadii{{sunRadius, planets[0].radius, planets[1].radius}};
inline constexpr std::array<double, 3> spinRates{{0.075, 0.25, 0.25}}; // Radians per simulated second.

inline Mat4 bodyTransform(Vec3 position, float radius, double time, double spinRate) {
    // Orbital azimuth increases from +X toward +Z, so prograde spin is about -Y.
    // Wrap each body's own angle, after applying its rate, to avoid discontinuities.
    const float angle = static_cast<float>(-std::fmod(time * spinRate, 2 * physicsPi));
    return translation(position) * rotationY(angle) * scale(radius);
}

inline double initialPeriod(const Planet& planet) {
    return 2 * physicsPi * std::sqrt(std::pow(planet.orbitRadius, 3) / (gravity * (sunMass + planet.mass)));
}

inline Bodies initialBodies(bool resonanceCandidate = false) {
    Bodies bodies{};
    bodies[0].mass = sunMass;
    for (std::size_t i = 0; i < planets.size(); ++i) {
        const auto& p = planets[i];
        if (!resonanceCandidate) {
            const double c = std::cos(p.initialAngle), s = std::sin(p.initialAngle);
            const double speed = std::sqrt(gravity * (sunMass + p.mass) / p.orbitRadius);
            bodies[i+1] = {p.mass, {p.orbitRadius*c, 0, p.orbitRadius*s}, {-speed*s, 0, speed*c}};
        } else {
            // An eccentric, phase-aligned 2:1 candidate. The same semimajor axes retain T2/T1 = 2.05.
            const double e = i == 0 ? 0.30 : 0.12;
            const double meanAnomaly = i == 0 ? 0.35 : 0.175; // Both periapsides are zero: 2*lambda2-lambda1 = 0.
            double anomaly = meanAnomaly;
            for (int n = 0; n < 15; ++n)
                anomaly -= (anomaly-e*std::sin(anomaly)-meanAnomaly) / (1-e*std::cos(anomaly));
            const double a = p.orbitRadius, c = std::cos(anomaly), s = std::sin(anomaly);
            const double beta = std::sqrt(1-e*e), distance = a*(1-e*c);
            const double velocityScale = std::sqrt(gravity*(sunMass+p.mass)*a)/distance;
            bodies[i+1] = {p.mass, {a*(c-e),0,a*beta*s}, {-velocityScale*s,0,velocityScale*beta*c}};
        }
    }
    // Start in the centre-of-mass frame, with no net linear momentum.
    const Vec3d center = centerOfMass(bodies);
    const Vec3d drift = momentum(bodies) * (1 / (sunMass + planets[0].mass + planets[1].mass));
    for (auto& body : bodies) { body.position = body.position - center; body.velocity = body.velocity - drift; }
    return bodies;
}

inline bool touching(const Bodies& bodies) {
    for (std::size_t i = 0; i < bodies.size(); ++i)
        for (std::size_t j = i + 1; j < bodies.size(); ++j)
            if (length(bodies[i].position - bodies[j].position) <= bodyRadii[i] + bodyRadii[j]) return true;
    return false;
}

struct Simulation {
    Bodies bodies = initialBodies();
    std::array<std::deque<Vec3>, 3> trails;
    OrbitDiagnostics diagnostics;
    double time = 0;
    double speed = defaultSpeed;
    bool paused = false, collision = false;
    bool resonanceCandidate = false;

    explicit Simulation(bool useResonanceCandidate = false)
        : bodies(initialBodies(useResonanceCandidate)), resonanceCandidate(useResonanceCandidate) {
        recordTrail(); diagnostics.record(bodies, time);
    }
    // A fixed step can also be used by tests and by eclipse seeking, independent of playback speed.
    bool step() {
        if (collision || touching(bodies)) { collision = paused = true; return false; }
        Bodies next = bodies;
        // Check contact after the drift, before the second force evaluation can become singular.
        kick(next, physicsStep * 0.5);
        drift(next, physicsStep);
        if (touching(next)) { collision = paused = true; return false; }
        kick(next, physicsStep * 0.5);
        bodies = next;
        time = ++steps_ * physicsStep;
        if (steps_ % 16 == 0) recordTrail(); // A trail sample every 1/16 simulated second.
        if (steps_ % OrbitDiagnostics::stride == 0) diagnostics.record(bodies, time);
        return true;
    }
    void advance(double realSeconds) {
        if (!paused && realSeconds > 0) integrate(realSeconds * speed * playbackScale);
    }
    void singleStep() { if (paused && !collision) integrate(0.125); }
    void faster() { speed = std::min(16.0, speed * 2); }
    void slower() { speed = std::max(1.0 / 64.0, speed / 2); }
    void reset() { *this = Simulation{resonanceCandidate}; }
    double energyError() const { return (energy(bodies) - initialEnergy_) / std::abs(initialEnergy_); }
    double angularMomentumError() const {
        return length(angularMomentum(bodies) - initialAngularMomentum_) / length(initialAngularMomentum_);
    }

private:
    std::uint64_t steps_ = 0;
    double pending_ = 0;
    double initialEnergy_ = energy(bodies);
    Vec3d initialAngularMomentum_ = angularMomentum(bodies);
    void integrate(double duration) {
        pending_ += duration;
        while (pending_ + 1e-12 >= physicsStep) {
            if (!step()) { pending_ = 0; return; }
            pending_ = std::max(0.0, pending_ - physicsStep);
        }
    }
    void recordTrail() {
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            trails[i].push_back(bodies[i].position.asFloat());
            if (trails[i].size() > 2048) trails[i].pop_front(); // Last 128 simulated seconds.
        }
    }
};

struct Camera {
    float yaw = 0.25f, pitch = 0.65f, distance = 24;
    Vec3 target{};
    Vec3 eye() const {
        return target + Vec3{distance * std::cos(pitch) * std::sin(yaw), distance * std::sin(pitch),
                             distance * std::cos(pitch) * std::cos(yaw)};
    }
    void rotate(float dx, float dy) {
        yaw -= dx * 0.006f;
        pitch = std::clamp(pitch + dy * 0.006f, -1.45f, 1.45f);
    }
    void zoom(float steps) { distance = std::clamp(distance * std::exp(-steps * 0.1f), 2.5f, 48.0f); }
};
} // namespace orbit
