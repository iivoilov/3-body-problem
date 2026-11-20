#include "stability.hpp"
#include <cstdlib>
#include <iostream>

using namespace orbit;
namespace {
void check(bool condition,const char* message) {
    if (!condition) {std::cerr << "FAIL: " << message << '\n';std::exit(EXIT_FAILURE);}
}
Vec3d planetAfter(const StabilityPreset& preset,double dt) {
    auto [stars,planet]=stabilityInitialState(preset);
    for (int i=0;i<static_cast<int>(1/dt);++i)
        check(advanceAlienState(stars,planet,dt),"convergence fixture remains collision-free");
    return planet.position;
}
}
int main() {
    for (const auto& preset:stabilityPresets) {
        const auto [stars,planet]=stabilityInitialState(preset);
        check(length(centerOfMass(stars))<2e-14,"candidate stellar barycentre is at origin");
        check(length(momentum(stars))<2e-14,"candidate stellar momentum is zero");
        const auto inner=innerStellarOrbit(stars),outer=outerStellarOrbit(stars);
        const auto p=stabilityOrbit(planet.position-stars[0].position,planet.velocity-stars[0].velocity,stars[0].mass);
        if (preset.current) {
            check(std::abs(mutualInclination(inner.angular,outer.angular)-6.6293228484229205)<1e-10,
                  "current viewer is already slightly noncoplanar");
            continue;
        }
        check(std::abs(inner.a-preset.innerA)<1e-12 && std::abs(inner.eccentricity-preset.innerE)<1e-12,
              "inner Jacobi orbit recovers the chosen elements");
        check(std::abs(outer.a-preset.outerA)<1e-11 && std::abs(outer.eccentricity-preset.outerE)<1e-12,
              "outer Jacobi orbit includes the tertiary mass and recovers its elements");
        check(std::abs(p.a-preset.planetA)<1e-12 && std::abs(p.eccentricity-preset.planetE)<1e-12,
              "planet starts at the specified host-relative orbit");
        check(std::abs(mutualInclination(inner.angular,outer.angular)-preset.outerInclination)<1e-5,
              "stellar mutual inclination is physically encoded in positions and velocities");
        check(std::abs(mutualInclination(inner.angular,p.angular)-preset.planetInclination)<1e-5,
              "planet inclination is relative to the inner binary");
        if (preset.outerInclination>0) {
            const Vec3d normal=cross(stars[1].position-stars[0].position,stars[2].position-stars[0].position);
            check(std::abs(dot(normal,planet.position-stars[0].position))/length(normal)>0.01,
                  "tilted candidate planet is outside the instantaneous three-star position plane");
        }
    }
    const auto& preset=stabilityPresets[4];
    const auto coarse=planetAfter(preset,1.0/128),fine=planetAfter(preset,1.0/256),reference=planetAfter(preset,1.0/512);
    const double ratio=length(fine-reference)/length(coarse-reference);
    check(ratio>0.15 && ratio<0.3,"inclined candidate converges at second order");
    const auto [current,planet]=stabilityInitialState(stabilityPresets[0]);
    const auto original=initialStars();
    for (std::size_t i=0;i<3;++i)
        check(length(current[i].position-original[i].position)==0,"current-scattering control preserves viewer state");
    check(length(planet.position-initialAlienPlanet(original).position)==0,"current planet state is unchanged");
    std::cout << "Stability initial conditions passed: Jacobi masses, orbital elements, true 3D geometry, barycentre, convergence, unchanged viewer.\n";
}
