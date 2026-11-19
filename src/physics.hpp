#pragma once

#include "math.hpp"
#include <array>

namespace orbit {
// Simulation units: length, mass, and time are arbitrary but consistent.
// GM gives a 24-second test-particle orbit at radius 5 around a mass of 1000.
inline constexpr double gravity = 0.00856736493150118;
inline constexpr double physicsStep = 1.0 / 256.0;
inline constexpr double defaultSpeed = 1;
inline constexpr double playbackScale = 4; // Simulated seconds per real second at displayed 1x.
inline constexpr double physicsPi = 3.14159265358979323846;

// Keep physics in double precision; convert to float only when drawing.
struct Vec3d {
    double x = 0, y = 0, z = 0;
    Vec3d operator+(Vec3d b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec3d operator-(Vec3d b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec3d operator*(double s) const { return {x*s, y*s, z*s}; }
    Vec3 asFloat() const { return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}; }
};
inline double dot(Vec3d a, Vec3d b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline double length(Vec3d a) { return std::sqrt(dot(a, a)); }
inline Vec3d cross(Vec3d a, Vec3d b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}

struct Body {
    double mass = 0;
    Vec3d position, velocity;
};
using Bodies = std::array<Body, 3>;

inline std::array<Vec3d, 3> accelerations(const Bodies& bodies) {
    std::array<Vec3d, 3> result{};
    for (std::size_t i = 0; i < bodies.size(); ++i)
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const Vec3d separation = bodies[j].position - bodies[i].position;
            const double r2 = dot(separation, separation);
            const Vec3d field = separation * (gravity / (r2 * std::sqrt(r2)));
            // Each pair contributes equal and opposite forces, including forces on the sun.
            result[i] = result[i] + field * bodies[j].mass;
            result[j] = result[j] - field * bodies[i].mass;
        }
    return result;
}

inline void kick(Bodies& bodies, double dt) {
    const auto acceleration = accelerations(bodies);
    for (std::size_t i = 0; i < bodies.size(); ++i)
        bodies[i].velocity = bodies[i].velocity + acceleration[i] * dt;
}
inline void drift(Bodies& bodies, double dt) {
    for (auto& body : bodies) body.position = body.position + body.velocity * dt;
}

// Kick-drift-kick leapfrog: both kicks are half steps, with forces recomputed after drift.
// Fixed steps keep this integrator second order, time reversible, and symplectic.
inline void leapfrogStep(Bodies& bodies, double dt) {
    kick(bodies, dt * 0.5);
    drift(bodies, dt);
    kick(bodies, dt * 0.5);
}

inline Vec3d momentum(const Bodies& bodies) {
    Vec3d result;
    for (const auto& body : bodies) result = result + body.velocity * body.mass;
    return result;
}
inline Vec3d centerOfMass(const Bodies& bodies) {
    Vec3d result;
    double mass = 0;
    for (const auto& body : bodies) { result = result + body.position * body.mass; mass += body.mass; }
    return result * (1 / mass);
}
inline Vec3d angularMomentum(const Bodies& bodies) {
    Vec3d result;
    for (const auto& body : bodies) result = result + cross(body.position, body.velocity) * body.mass;
    return result;
}
inline double energy(const Bodies& bodies) {
    double result = 0;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        result += 0.5 * bodies[i].mass * dot(bodies[i].velocity, bodies[i].velocity);
        for (std::size_t j = i + 1; j < bodies.size(); ++j)
            result -= gravity * bodies[i].mass * bodies[j].mass / length(bodies[j].position - bodies[i].position);
    }
    return result;
}
} // namespace orbit
