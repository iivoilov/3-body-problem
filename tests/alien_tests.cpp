#include "alien_world.hpp"
#include <cstdlib>
#include <iostream>

using namespace orbit;
namespace {
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(EXIT_FAILURE); }
}
std::pair<Bodies, Body> integrate(double dt) {
    auto stars = initialStars();
    auto planet = initialAlienPlanet(stars);
    for (int i = 0; i < static_cast<int>(0.5/dt); ++i)
        check(advanceAlienState(stars, planet, dt), "short convergence fixture avoids contact");
    return {stars, planet};
}
}
int main() {
    AlienSimulation initial;
    check(length(momentum(initial.stars)) < 1e-14, "initial stellar momentum is zero");
    check(length(centerOfMass(initial.stars)) < 1e-14, "initial stellar barycentre is at the origin");
    check(initial.planet.mass == 0, "planet is a test particle");
    const auto directions = alienSkyDirections(initial.stars, initial.planet, 0);
    for (auto direction : directions) check(std::abs(length(direction)-1) < 1e-6, "surface directions are unit vectors");
    auto displaced = initial;
    displaced.planet.position = displaced.planet.position + Vec3d{0.2,0.1,0.3};
    initial.step(); displaced.step();
    for (std::size_t i = 0; i < 3; ++i)
        check(length(initial.stars[i].position-displaced.stars[i].position) == 0, "test particle exerts no stellar back reaction");

    const auto coarse = integrate(1.0/2048), fine = integrate(1.0/4096), reference = integrate(1.0/8192);
    const double coarseError = length(coarse.second.position-reference.second.position);
    const double fineError = length(fine.second.position-reference.second.position);
    check(fineError < coarseError*0.30 && fineError > coarseError*0.15, "planet trajectory converges at second order");

    AlienSimulation one, partitioned;
    one.advance(2);
    for (int i = 0; i < 256; ++i) partitioned.advance(2.0/256);
    check(one.time == partitioned.time && length(one.planet.position-partitioned.planet.position) < 1e-12,
          "frame partitioning does not change the trajectory");
    one.paused = true;
    const double before = one.time;
    one.advance(20);
    check(one.time == before, "pause freezes time");
    one.singleStep();
    check(one.time == before+16*alienStep && one.paused, "single step preserves pause");

    AlienSimulation run;
    double maxError = 0, minFlux = 1e30, maxFlux = 0, minSeparation = 1e30;
    for (int i = 0; i < static_cast<int>(36/alienStep); ++i) {
        check(run.step(), "36-year reference run avoids physical contact");
        if (i%64 != 0) continue;
        const auto sample = run.measure();
        check(std::isfinite(sample.flux) && sample.flux > 0, "irradiance remains finite and positive");
        check(std::abs(sample.flux-sample.contributions[0]-sample.contributions[1]-sample.contributions[2]) < 1e-12,
              "irradiance sums the three inverse-square contributions");
        maxError = std::max(maxError, std::abs(sample.energyError));
        minFlux = std::min(minFlux, sample.flux); maxFlux = std::max(maxFlux, sample.flux);
        minSeparation = std::min(minSeparation, sample.separation);
    }
    check(maxError < 2e-5, "stellar energy error stays below 2e-5 in the full reference run");
    check(run.measure().angularError < 1e-10 && length(momentum(run.stars)) < 1e-10,
          "stellar angular and linear momentum are conserved");
    check(length(centerOfMass(run.stars)) < 1e-9, "stellar barycentre does not drift");
    check(maxFlux > minFlux*5 && minSeparation < 1, "chosen run has varying light and a close stellar passage");
    check(run.history.size() == 2048 && run.trails[3].size() == 2048, "recorded history and trails are bounded");

    AlienSimulation contact;
    contact.planet.position = contact.stars[0].position;
    const auto star = contact.stars[0].position;
    check(!contact.step() && contact.collision && contact.paused && contact.time == 0,
          "planet contact pauses before singular force evaluation");
    check(length(contact.stars[0].position-star) == 0, "contact retains the last state");
    contact.reset();
    check(!contact.collision && !contact.paused && contact.time == 0, "reset restores the initial world");
    contact.stars[2].position = contact.stars[1].position;
    check(!contact.step(), "stellar contact also pauses safely");
    std::cout << "Orbit / 02 passed: 36 years; max stellar energy error " << maxError
              << "; flux " << minFlux << ".." << maxFlux << "; closest stellar pair " << minSeparation << " AU.\n";
}
