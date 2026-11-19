#pragma once

#include "physics.hpp"

namespace orbit {
// Equilibrium tide, constant time lag (Mignard/Hut). Only tides raised by
// body 0 in bodies 1 and 2 are included. The primary remains spherical.
// These radii are physical model parameters, independent of drawing radii.
struct TidalPlanet {
    double radius = 0, loveNumber = 0, lag = 0, inertiaFactor = 0.33;
    double inertia(double mass) const { return inertiaFactor * mass * radius * radius; }
    double coefficient(double primaryMass) const {
        return 3 * gravity * loveNumber * primaryMass * primaryMass * std::pow(radius, 5);
    }
};
using TidalPlanets = std::array<TidalPlanet, 2>;

struct TidalState {
    Bodies bodies;
    std::array<Vec3d, 2> spins{}; // Angular velocity vectors, not prescribed animation.
    double heat = 0;
};

inline double tidalPotential(const Bodies& bodies, const TidalPlanets& parameters) {
    double potential = 0;
    for (std::size_t i = 0; i < 2; ++i) {
        if (parameters[i].loveNumber == 0) continue;
        const double r2 = dot(bodies[i+1].position-bodies[0].position,
                              bodies[i+1].position-bodies[0].position);
        potential -= parameters[i].coefficient(bodies[0].mass) / (6 * r2*r2*r2);
    }
    return potential;
}

inline double tidalMechanicalEnergy(const TidalState& state, const TidalPlanets& parameters) {
    double value = energy(state.bodies) + tidalPotential(state.bodies, parameters);
    for (std::size_t i = 0; i < 2; ++i)
        value += 0.5 * parameters[i].inertia(state.bodies[i+1].mass) * dot(state.spins[i], state.spins[i]);
    return value;
}

inline Vec3d tidalAngularMomentum(const TidalState& state, const TidalPlanets& parameters) {
    Vec3d value = angularMomentum(state.bodies);
    for (std::size_t i = 0; i < 2; ++i)
        value = value + state.spins[i] * parameters[i].inertia(state.bodies[i+1].mass);
    return value;
}

inline void tidalConservativeKick(TidalState& state, const TidalPlanets& parameters, double dt) {
    auto acceleration = accelerations(state.bodies);
    for (std::size_t i = 0; i < 2; ++i) {
        const auto& p = parameters[i];
        if (p.loveNumber == 0) continue;
        const Vec3d r = state.bodies[i+1].position-state.bodies[0].position;
        const double r2 = dot(r,r), r8 = r2*r2*r2*r2;
        const Vec3d force = r * (-p.coefficient(state.bodies[0].mass) / r8);
        acceleration[i+1] = acceleration[i+1] + force * (1 / state.bodies[i+1].mass);
        acceleration[0] = acceleration[0] - force * (1 / state.bodies[0].mass);
    }
    for (std::size_t i = 0; i < 3; ++i)
        state.bodies[i].velocity = state.bodies[i].velocity + acceleration[i] * dt;
}

// Exact dissipative evolution for one pair while positions are held fixed.
// F_planet = -A [2 r (r.v)/r^2 + v - Omega x r], A = 3 G k2 M^2 R^5 lag/r^8.
// Spin torque is -r x F; back reaction is applied to the primary.
// Radial velocity and tangential slip each decay exponentially. This avoids
// treating a velocity-dependent force as an ordinary leapfrog kick.
inline void tidalPairDissipation(TidalState& state, const TidalPlanets& parameters,
                                 std::size_t i, double dt) {
    const auto& p = parameters[i];
    if (p.loveNumber == 0 || p.lag == 0) return;
    auto& primary = state.bodies[0];
    auto& planet = state.bodies[i+1];
    const Vec3d r = planet.position-primary.position, v = planet.velocity-primary.velocity;
    const double r2 = dot(r,r), reducedMass = primary.mass*planet.mass/(primary.mass+planet.mass);
    const double inertia = p.inertia(planet.mass);
    const double a = p.coefficient(primary.mass)*p.lag / (r2*r2*r2*r2);
    const Vec3d radial = r * (dot(r,v)/r2);
    const Vec3d slip = v-radial-cross(state.spins[i], r);
    const double coupling = 1 + reducedMass*r2/inertia;
    const double radialRate = 3*a/reducedMass, slipRate = a*coupling/reducedMass;
    const Vec3d deltaRadial = radial * std::expm1(-radialRate*dt);
    const Vec3d deltaTangential = slip * (std::expm1(-slipRate*dt)/coupling);
    const Vec3d delta = deltaRadial+deltaTangential;
    planet.velocity = planet.velocity + delta * (reducedMass/planet.mass);
    primary.velocity = primary.velocity - delta * (reducedMass/primary.mass);
    state.spins[i] = state.spins[i] - cross(r,deltaTangential) * (reducedMass/inertia);
    state.heat += 0.5*reducedMass*dot(radial,radial)*(-std::expm1(-2*radialRate*dt))
                + 0.5*reducedMass/coupling*dot(slip,slip)*(-std::expm1(-2*slipRate*dt));
}

inline void tidalDissipation(TidalState& state, const TidalPlanets& parameters, double dt) {
    // Symmetric composition because both pairs exchange momentum with body 0.
    tidalPairDissipation(state, parameters, 0, dt/2);
    tidalPairDissipation(state, parameters, 1, dt);
    tidalPairDissipation(state, parameters, 0, dt/2);
}

inline bool tidalContact(const Bodies& bodies, const TidalPlanets& parameters) {
    const std::array<double, 3> radii{{1.0, parameters[0].radius, parameters[1].radius}};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = i+1; j < 3; ++j)
            if (length(bodies[j].position-bodies[i].position) <= radii[i]+radii[j]) return true;
    return false;
}

// Strang split, second order: dissipation / conservative leapfrog / dissipation.
// Mechanical energy falls; mechanical energy + accumulated heat is the budget.
inline bool tidalStep(TidalState& state, const TidalPlanets& parameters, double dt) {
    if (tidalContact(state.bodies, parameters)) return false;
    TidalState next = state;
    tidalDissipation(next, parameters, dt/2);
    tidalConservativeKick(next, parameters, dt/2);
    drift(next.bodies, dt);
    if (tidalContact(next.bodies, parameters)) return false;
    tidalConservativeKick(next, parameters, dt/2);
    tidalDissipation(next, parameters, dt/2);
    state = next;
    return true;
}
} // namespace orbit
