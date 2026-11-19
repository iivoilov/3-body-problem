#include "scene.hpp"
#include "tides.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace orbit;

int main(int argc, char** argv) {
    try {
        std::string preset = "circular", tides = "both", output = "tidal.csv";
        double duration = 1048576, dt = 1.0/32, interval = 8, lag1 = 0.1, lag2 = 0.1;
        double radius1 = 0.15, radius2 = 0.15*std::cbrt(2.0);
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "tidal_experiment [--preset circular|resonance] [--tides none|elastic|inner|outer|both]\n"
                             "  [--duration seconds] [--step seconds] [--interval seconds]\n"
                             "  [--lag-inner seconds] [--lag-outer seconds] [--output path.csv]\n";
                std::cout << "  [--radius-inner length] [--radius-outer length]\n";
                return 0;
            }
            if (i+1 >= argc) throw std::runtime_error("Missing value for " + arg);
            const std::string value = argv[++i];
            if (arg == "--preset") preset = value;
            else if (arg == "--tides") tides = value;
            else if (arg == "--output") output = value;
            else if (arg == "--duration") duration = std::stod(value);
            else if (arg == "--step") dt = std::stod(value);
            else if (arg == "--interval") interval = std::stod(value);
            else if (arg == "--lag-inner") lag1 = std::stod(value);
            else if (arg == "--lag-outer") lag2 = std::stod(value);
            else if (arg == "--radius-inner") radius1 = std::stod(value);
            else if (arg == "--radius-outer") radius2 = std::stod(value);
            else throw std::runtime_error("Unknown option " + arg);
        }
        if (preset != "circular" && preset != "resonance") throw std::runtime_error("Unknown preset");
        if (tides != "none" && tides != "elastic" && tides != "inner" && tides != "outer" && tides != "both")
            throw std::runtime_error("Unknown tides mode");
        if (!std::isfinite(duration) || !std::isfinite(dt) || !std::isfinite(interval) ||
            !std::isfinite(lag1) || !std::isfinite(lag2) || duration <= 0 || dt <= 0 || dt > 0.5 ||
            interval < 0.5 || lag1 < 0 || lag2 < 0 || !std::isfinite(radius1) || !std::isfinite(radius2) ||
            radius1 <= 0 || radius2 <= 0)
            throw std::runtime_error("Require positive duration, 0 < step <= 0.5, interval >= 0.5, nonnegative lags");
        const auto count = static_cast<std::uint64_t>(std::llround(duration/dt));
        const auto diagnosticStride = static_cast<std::uint64_t>(std::llround(0.5/dt));
        const auto outputStride = static_cast<std::uint64_t>(std::llround(interval/dt));
        if (std::abs(count*dt-duration) > 1e-8 || std::abs(diagnosticStride*dt-0.5) > 1e-10 ||
            std::abs(outputStride*dt-interval) > 1e-10 || outputStride % diagnosticStride != 0)
            throw std::runtime_error("Step must divide duration and 0.5; interval must be a multiple of 0.5");

        // Equal-density compact toy planets, not the exaggerated display spheres.
        TidalPlanets parameters{{{radius1, 0.3, lag1, 0.33}, {radius2, 0.3, lag2, 0.33}}};
        if (tides == "none") for (auto& p : parameters) p.loveNumber = 0;
        if (tides == "elastic") for (auto& p : parameters) p.lag = 0;
        if (tides == "inner") parameters[1].loveNumber = 0;
        if (tides == "outer") parameters[0].loveNumber = 0;
        TidalState state{initialBodies(preset == "resonance"), {{{0,-spinRates[1],0}, {0,-spinRates[2],0}}}, 0};
        const double initialEnergy = tidalMechanicalEnergy(state, parameters);
        const Vec3d initialAngular = tidalAngularMomentum(state, parameters);
        double maxEnergyError = 0, maxAngularError = 0, maxMomentum = 0, maxLag = 0;
        OrbitDiagnostics diagnostics;
        std::ofstream csv(output);
        if (!csv) throw std::runtime_error("Cannot open " + output);
        csv << std::setprecision(15)
            << "time,period_ratio,a_azure,a_ember,e_azure,e_ember,phi_azure_turns,phi_ember_turns,"
               "segment_azure,segment_ember,spin_over_n_azure,spin_over_n_ember,mechanical_energy,heat,"
               "energy_budget_error,angular_momentum_error,net_momentum,lag_phase_bound\n";
        auto sample = [&](double time, bool write) {
            diagnostics.record(state.bodies, time);
            const auto& s = diagnostics.history.back();
            const double mechanical = tidalMechanicalEnergy(state, parameters);
            const double energyError = (mechanical+state.heat-initialEnergy)/std::abs(initialEnergy);
            const double angularError = length(tidalAngularMomentum(state, parameters)-initialAngular)/length(initialAngular);
            const double netMomentum = length(momentum(state.bodies));
            double lagBound = 0;
            for (std::size_t i = 0; i < 2; ++i) {
                if (parameters[i].loveNumber == 0) continue;
                const Vec3d r = state.bodies[i+1].position-state.bodies[0].position;
                const Vec3d v = state.bodies[i+1].velocity-state.bodies[0].velocity;
                // Conservative upper bound on quadrupole lag: orbital + spin frequencies.
                lagBound = std::max(lagBound, 2*parameters[i].lag*(length(cross(r,v))/dot(r,r)+length(state.spins[i])));
            }
            maxEnergyError = std::max(maxEnergyError, std::abs(energyError));
            maxAngularError = std::max(maxAngularError, angularError);
            maxMomentum = std::max(maxMomentum, netMomentum);
            maxLag = std::max(maxLag, lagBound);
            if (!s.ratioValid) throw std::runtime_error("Unbound orbit at " + std::to_string(time));
            if (!write) return;
            csv << time << ',' << s.periodRatio;
            for (const auto& e : s.elements) csv << ',' << e.semimajorAxis;
            for (const auto& e : s.elements) csv << ',' << e.eccentricity;
            for (int i = 0; i < 2; ++i) {
                csv << ',';
                if (s.phaseValid[i]) csv << s.phaseTurns[i];
            }
            for (const auto segment : s.phaseSegment) csv << ',' << segment;
            for (int i = 0; i < 2; ++i) csv << ',' << -state.spins[i].y*s.elements[i].period/(2*physicsPi);
            csv << ',' << mechanical << ',' << state.heat << ',' << energyError << ',' << angularError
                << ',' << netMomentum << ',' << lagBound << '\n';
        };
        sample(0, true);
        for (std::uint64_t step = 1; step <= count; ++step) {
            if (!tidalStep(state, parameters, dt)) throw std::runtime_error("Contact at " + std::to_string(step*dt));
            if (step % diagnosticStride == 0 || step == count)
                sample(step*dt, step % outputStride == 0 || step == count);
        }
        if (!csv) throw std::runtime_error("Failed writing " + output);
        std::cout << std::setprecision(10) << preset << '/' << tides << ": " << duration << " seconds, step " << dt
                  << "; heat " << state.heat << "; max |E+heat-E0|/|E0| " << maxEnergyError
                  << "; max relative total L error " << maxAngularError << "; max net momentum " << maxMomentum
                  << "; max lag bound " << maxLag << "; CSV " << output << '\n';
    } catch (const std::exception& error) {
        std::cerr << "tidal_experiment: " << error.what() << '\n';
        return 1;
    }
}
