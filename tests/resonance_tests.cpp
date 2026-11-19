#include "scene.hpp"
#include <cstdlib>
#include <iostream>

using namespace orbit;
namespace {
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(EXIT_FAILURE); }
}
}

int main() {
    Simulation simulation{true};
    const auto& start = simulation.diagnostics.history.back();
    check(simulation.resonanceCandidate && start.ratioValid && std::abs(start.periodRatio-2.05) < 1e-6,
          "candidate retains the requested 2.05 initial period ratio");
    check(std::abs(start.elements[0].eccentricity-0.30) < 1e-12 &&
          std::abs(start.elements[1].eccentricity-0.12) < 1e-12, "candidate has the tested eccentricities");
    check(start.phaseValid[0] && start.phaseValid[1] && std::abs(start.phaseTurns[0]) < 1e-12 &&
          std::abs(start.phaseTurns[1]) < 1e-12, "candidate starts with aligned critical angles");
    check(length(momentum(simulation.bodies)) < 1e-13 && length(centerOfMass(simulation.bodies)) < 1e-14,
          "candidate preserves zero initial net momentum and barycentre");
    simulation.advance(1);
    check(simulation.time == 4, "candidate obeys four-times-faster 1x playback");
    simulation.reset();
    check(simulation.resonanceCandidate && simulation.time == 0 && simulation.diagnostics.history.size() == 1 &&
          std::abs(simulation.diagnostics.history.front().elements[0].eccentricity-0.30) < 1e-12,
          "reset retains the selected starting preset");

    std::array<double,2> low{{1e9,1e9}}, high{{-1e9,-1e9}};
    std::array<std::uint64_t,2> segment{};
    double maxEnergyError = 0;
    while (simulation.time < 16384) {
        check(simulation.step(), "candidate remains separated during the long observation");
        if (simulation.diagnostics.history.back().time != simulation.time) continue;
        maxEnergyError = std::max(maxEnergyError, std::abs(simulation.energyError()));
        const auto& sample = simulation.diagnostics.history.back();
        for (int i = 0; i < 2; ++i) {
            check(sample.phaseValid[i], "critical angle stays defined throughout the observation window");
            check(segment[i] == 0 || segment[i] == sample.phaseSegment[i], "no hidden resets mask phase circulation");
            segment[i] = sample.phaseSegment[i];
            low[i] = std::min(low[i], sample.phaseTurns[i]);
            high[i] = std::max(high[i], sample.phaseTurns[i]);
        }
    }
    check(high[0]-low[0] < 0.25 && high[1]-low[1] < 0.5,
          "both 2:1 angles remain bounded over 16384 simulated seconds");
    check(maxEnergyError < 1e-6, "candidate retains bounded numerical energy error");
    check(simulation.angularMomentumError() < 1e-10 && length(momentum(simulation.bodies)) < 1e-10,
          "candidate preserves angular and linear momentum");
    std::cout << "Resonance candidate: 16384 simulated seconds, phase spans " << high[0]-low[0] << " and "
              << high[1]-low[1] << " turns; max relative energy error " << maxEnergyError << ".\n";
}
