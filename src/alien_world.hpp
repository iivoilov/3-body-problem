#pragma once

#include "physics.hpp"
#include <cstdint>
#include <deque>

namespace orbit {
// AU, solar masses, Julian years. The mass reference is real; this compact
// arrangement, the planet, slow rotation, and surface latitude are invented.
inline constexpr double stellarGravity = 4 * physicsPi * physicsPi;
inline constexpr double alienStep = 1.0 / 16384.0;
inline constexpr double alienPlayback = 0.08; // Years per real second at 1x.
inline constexpr double alienRotationPeriod = 1.6;
inline constexpr double alienPlanetRadius = 0.000042635;
inline constexpr std::array<double, 3> stellarMasses{{1.0788, 0.9092, 0.1221}};
inline constexpr std::array<double, 3> stellarRadii{{0.005662, 0.003995, 0.000656}};
inline constexpr std::array<double, 3> stellarLuminosities{{1.5059, 0.4981, 0.0016}};
inline constexpr std::array<Vec3, 3> stellarColors{{{1.0f, 0.80f, 0.48f}, {1.0f, 0.43f, 0.16f}, {1.0f, 0.13f, 0.065f}}};
inline constexpr std::array<float, 3> stellarDisplayRadii{{0.34f, 0.27f, 0.18f}};
inline constexpr std::array<const char*, 3> stellarNames{{"ALPHA", "BETA", "PROXIMA"}};

inline Bodies initialStars() {
    const double pairMass = stellarMasses[0] + stellarMasses[1];
    const double separation = 4.0;
    const double binarySpeed = std::sqrt(stellarGravity * pairMass / separation);
    Bodies stars{{
        {stellarMasses[0], {-separation*stellarMasses[1]/pairMass, 0, 0}, {0, 0, -binarySpeed*stellarMasses[1]/pairMass}},
        {stellarMasses[1], { separation*stellarMasses[0]/pairMass, 0, 0}, {0, 0,  binarySpeed*stellarMasses[0]/pairMass}},
        {stellarMasses[2], {-1.5, 0.65, 6.0}, {-3.1, -0.15, -0.7}}
    }};
    const Vec3d center = centerOfMass(stars);
    const Vec3d velocity = momentum(stars) * (1 / (pairMass + stellarMasses[2]));
    for (auto& star : stars) {
        star.position = star.position - center;
        star.velocity = star.velocity - velocity;
    }
    return stars;
}

inline Body initialAlienPlanet(const Bodies& stars) {
    const Vec3d radial{-std::sqrt(0.5), 0, -std::sqrt(0.5)};
    // Start at the apoapsis of an osculating a=0.90 AU, e=0.16 orbit about A.
    const double radius = 0.90 * 1.16;
    const double speed = std::sqrt(stellarGravity * stars[0].mass * 0.84 / radius);
    return {0, stars[0].position + radial * radius,
            stars[0].velocity + Vec3d{-radial.z, 0, radial.x} * speed};
}

inline Vec3d planetAcceleration(const Bodies& stars, Vec3d position) {
    Vec3d acceleration;
    for (const auto& star : stars) {
        const Vec3d r = star.position - position;
        const double distance = length(r);
        acceleration = acceleration + r * (stellarGravity * star.mass / (distance*distance*distance));
    }
    return acceleration;
}

inline bool alienContact(const Bodies& stars, const Body& planet) {
    for (std::size_t i = 0; i < 3; ++i) {
        if (length(stars[i].position - planet.position) <= stellarRadii[i] + alienPlanetRadius) return true;
        for (std::size_t j = i+1; j < 3; ++j)
            if (length(stars[i].position - stars[j].position) <= stellarRadii[i] + stellarRadii[j]) return true;
    }
    return false;
}

inline bool advanceAlienState(Bodies& stars, Body& planet, double dt) {
    if (alienContact(stars, planet)) return false;
    Bodies next = stars;
    Body nextPlanet = planet;
    // Reuse the pairwise Newtonian force solver, converting its G to AU/Msun/yr.
    kick(next, dt * 0.5 * stellarGravity / gravity);
    nextPlanet.velocity = nextPlanet.velocity + planetAcceleration(stars, planet.position) * (dt * 0.5);
    drift(next, dt);
    nextPlanet.position = nextPlanet.position + nextPlanet.velocity * dt;
    if (alienContact(next, nextPlanet)) return false;
    kick(next, dt * 0.5 * stellarGravity / gravity);
    nextPlanet.velocity = nextPlanet.velocity + planetAcceleration(next, nextPlanet.position) * (dt * 0.5);
    stars = next;
    planet = nextPlanet;
    return true;
}

inline double stellarEnergy(const Bodies& stars) {
    double result = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        result += stars[i].mass * dot(stars[i].velocity, stars[i].velocity) * 0.5;
        for (std::size_t j = i+1; j < 3; ++j)
            result -= stellarGravity * stars[i].mass * stars[j].mass / length(stars[i].position-stars[j].position);
    }
    return result;
}

struct AlienSample {
    double time = 0, flux = 0, nearest = 0, separation = 0;
    double energyError = 0, angularError = 0;
    std::array<double, 3> contributions{};
};

class AlienSimulation {
public:
    Bodies stars = initialStars();
    Body planet = initialAlienPlanet(stars); // Massless: feels the stars, exerts no back reaction.
    std::array<std::deque<Vec3>, 4> trails;
    std::deque<AlienSample> history;
    double time = 0, speed = 1;
    bool paused = false, collision = false;

    AlienSimulation() { record(); }
    bool step() {
        if (collision || !advanceAlienState(stars, planet, alienStep)) {
            collision = paused = true;
            return false;
        }
        time = ++steps_ * alienStep;
        if (steps_ % 64 == 0) record();
        return true;
    }
    void advance(double seconds) {
        if (paused || seconds <= 0) return;
        integrate(seconds * alienPlayback * speed);
    }
    void singleStep() { if (paused && !collision) integrate(16 * alienStep); }
    void faster() { speed = std::min(16.0, speed * 2); }
    void slower() { speed = std::max(1.0/64, speed * 0.5); }
    void reset() { *this = AlienSimulation{}; }
    AlienSample measure() const {
        AlienSample sample;
        sample.time = time;
        sample.nearest = sample.separation = 1e30;
        for (std::size_t i = 0; i < 3; ++i) {
            const double distance = length(stars[i].position - planet.position);
            sample.contributions[i] = stellarLuminosities[i] / (distance * distance);
            sample.flux += sample.contributions[i];
            sample.nearest = std::min(sample.nearest, distance);
            for (std::size_t j = i+1; j < 3; ++j)
                sample.separation = std::min(sample.separation, length(stars[i].position-stars[j].position));
        }
        sample.energyError = (stellarEnergy(stars)-initialEnergy_) / std::abs(initialEnergy_);
        sample.angularError = length(angularMomentum(stars)-initialAngular_) / length(initialAngular_);
        return sample;
    }
private:
    std::uint64_t steps_ = 0;
    double pending_ = 0;
    double initialEnergy_ = stellarEnergy(initialStars());
    Vec3d initialAngular_ = angularMomentum(initialStars());
    void integrate(double duration) {
        pending_ += duration;
        while (pending_ + 1e-14 >= alienStep) {
            if (!step()) { pending_ = 0; return; }
            pending_ = std::max(0.0, pending_-alienStep);
        }
    }
    void record() {
        for (std::size_t i = 0; i < 4; ++i) {
            trails[i].push_back((i < 3 ? stars[i].position : planet.position).asFloat());
            if (trails[i].size() > 2048) trails[i].pop_front();
        }
        history.push_back(measure());
        if (history.size() > 2048) history.pop_front();
    }
};

// A slow rotating, high-latitude observer. Terrain is an artistic local patch;
// sun directions are derived from the integrated stellar/planetary positions.
inline std::array<Vec3, 3> alienSkyDirections(const Bodies& stars, const Body& planet, double time) {
    const double angle = 0.57 + 2 * physicsPi * time / alienRotationPeriod;
    const Vec3 up{static_cast<float>(0.28 * std::sin(angle)), std::sqrt(1-0.28f*0.28f),
                  static_cast<float>(0.28 * std::cos(angle))};
    const Vec3 north = normalized(Vec3{0,1,0} - up * up.y);
    const Vec3 east = normalized(cross(north, up));
    std::array<Vec3, 3> result;
    for (std::size_t i = 0; i < 3; ++i) {
        const Vec3 direction = normalized((stars[i].position - planet.position).asFloat());
        result[i] = {dot(direction, east), dot(direction, up), dot(direction, north)};
    }
    return result;
}
} // namespace orbit
