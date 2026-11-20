#include "stability.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace orbit;
int main(int argc,char** argv) {
    try {
        if (argc<2) throw std::runtime_error("Usage: stellar_stability PRESET [--years N] [--step N] [--output FILE] [--phase N] [--inclination-offset N]");
        StabilityPreset preset{};
        bool found=false;
        for (const auto& candidate:stabilityPresets) if (candidate.name==std::string(argv[1])) { preset=candidate; found=true; }
        if (!found) throw std::runtime_error("Unknown stability preset.");
        double years=1000000, dt=1.0/512, phase=0, inclinationOffset=0;
        std::string outputPath;
        for (int i=2;i<argc;i+=2) {
            if (i+1==argc) throw std::runtime_error("Missing option value.");
            const std::string option=argv[i];
            if (option=="--output") outputPath=argv[i+1];
            else {
                std::size_t consumed;
                const double value=std::stod(argv[i+1],&consumed);
                if (consumed!=std::string(argv[i+1]).size() || !std::isfinite(value)) throw std::runtime_error("Invalid numeric value.");
                if (option=="--years") years=value;
                else if (option=="--step") dt=value;
                else if (option=="--phase") phase=value;
                else if (option=="--inclination-offset") inclinationOffset=value;
                else throw std::runtime_error("Unknown option: "+option);
            }
        }
        if (years<=0 || dt<=0 || dt>0.01 || years/dt>1e12) throw std::runtime_error("Invalid duration or timestep.");
        preset.outerPhase+=phase;
        preset.planetPhase+=phase;
        preset.outerInclination+=inclinationOffset;
        auto [stars,planet]=stabilityInitialState(preset);
        const auto initialStarsState=stars;
        const auto initialPlanet=planet;
        const double initialEnergy=stellarEnergy(stars);
        const Vec3d initialAngular=angularMomentum(stars);
        const double initialInnerA=innerStellarOrbit(stars).a, initialOuterA=outerStellarOrbit(stars).a;
        const auto initialPlanetOrbit=stabilityOrbit(planet.position-stars[0].position,planet.velocity-stars[0].velocity,stars[0].mass);
        const double initialPlanetA=initialPlanetOrbit.a;
        const double initialMutual=mutualInclination(innerStellarOrbit(stars).angular,outerStellarOrbit(stars).angular);
        const double initialPlanetInclination=mutualInclination(innerStellarOrbit(stars).angular,initialPlanetOrbit.angular);
        const std::uint64_t count=static_cast<std::uint64_t>(std::ceil(years/dt));
        const std::uint64_t checkStride=std::max<std::uint64_t>(1,std::llround(0.125/dt));
        const std::uint64_t outputStride=std::max<std::uint64_t>(1,std::llround(128/dt));
        std::ofstream csv;
        if (!outputPath.empty()) {
            csv.open(outputPath);
            if (!csv) throw std::runtime_error("Cannot open CSV output.");
            csv << "time,star_span,inner_a,inner_e,outer_a,outer_e,planet_a,planet_e,mutual_i,planet_i,flux,stellar_energy_error,stellar_angular_error\n" << std::setprecision(17);
        }
        double time=0, maxSpan=0, minStarSeparation=1e30, maxEnergy=0, maxAngular=0, maxMomentum=0, maxBarycentre=0;
        double minPlanetA=1e30,maxPlanetA=0,minPlanetDistance=1e30,maxPlanetDistance=0,maxPlanetE=0;
        double minInnerA=1e30,maxInnerA=0,minOuterA=1e30,maxOuterA=0,maxInnerE=0,maxOuterE=0;
        double minFlux=1e30,maxFlux=0,minMutual=180,maxMutual=0,minPlanetI=180,maxPlanetI=0;
        std::string failure="none";
        std::uint64_t steps=0,checks=0;
        const auto start=std::chrono::steady_clock::now();
        auto measure=[&](bool write) {
            ++checks;
            const auto inner=innerStellarOrbit(stars), outer=outerStellarOrbit(stars);
            const auto p=stabilityOrbit(planet.position-stars[0].position,planet.velocity-stars[0].velocity,stars[0].mass);
            const double mutual=mutualInclination(inner.angular,outer.angular), planetI=mutualInclination(inner.angular,p.angular);
            const double energyError=(stellarEnergy(stars)-initialEnergy)/std::abs(initialEnergy);
            const double angularError=length(angularMomentum(stars)-initialAngular)/length(initialAngular);
            double span=0,flux=0;
            for (std::size_t i=0;i<3;++i) {
                const double r=length(stars[i].position-planet.position);
                flux+=(preset.equalMasses ? 1.0 : stellarLuminosities[i])/(r*r);
                for (std::size_t j=i+1;j<3;++j) {
                    const double separation=length(stars[i].position-stars[j].position);
                    span=std::max(span,separation); minStarSeparation=std::min(minStarSeparation,separation);
                }
            }
            const double distance=length(planet.position-stars[0].position);
            maxSpan=std::max(maxSpan,span); maxEnergy=std::max(maxEnergy,std::abs(energyError)); maxAngular=std::max(maxAngular,angularError);
            maxMomentum=std::max(maxMomentum,length(momentum(stars))); maxBarycentre=std::max(maxBarycentre,length(centerOfMass(stars)));
            minPlanetA=std::min(minPlanetA,p.a); maxPlanetA=std::max(maxPlanetA,p.a); maxPlanetE=std::max(maxPlanetE,p.eccentricity);
            minPlanetDistance=std::min(minPlanetDistance,distance); maxPlanetDistance=std::max(maxPlanetDistance,distance);
            minInnerA=std::min(minInnerA,inner.a); maxInnerA=std::max(maxInnerA,inner.a); maxInnerE=std::max(maxInnerE,inner.eccentricity);
            minOuterA=std::min(minOuterA,outer.a); maxOuterA=std::max(maxOuterA,outer.a); maxOuterE=std::max(maxOuterE,outer.eccentricity);
            minFlux=std::min(minFlux,flux); maxFlux=std::max(maxFlux,flux);
            minMutual=std::min(minMutual,mutual); maxMutual=std::max(maxMutual,mutual);
            minPlanetI=std::min(minPlanetI,planetI); maxPlanetI=std::max(maxPlanetI,planetI);
            if (write && csv.is_open()) csv << time << ',' << span << ',' << inner.a << ',' << inner.eccentricity << ',' << outer.a << ',' << outer.eccentricity << ',' << p.a << ',' << p.eccentricity << ',' << mutual << ',' << planetI << ',' << flux << ',' << energyError << ',' << angularError << '\n';
            if (!std::isfinite(span) || !std::isfinite(p.a)) failure="nonfinite_state";
            else if (span>100) failure="stars_exceed_100_AU";
            else if (inner.specificEnergy>=0 || outer.specificEnergy>=0) failure="stellar_hierarchy_not_bound";
            else if (inner.a<initialInnerA*0.5 || inner.a>initialInnerA*2 || outer.a<initialOuterA*0.5 || outer.a>initialOuterA*2) failure="stellar_semimajor_axis_outside_factor_two";
            else if (p.specificEnergy>=0) failure="planet_not_bound_to_A";
            else if (distance>5 || p.a<initialPlanetA*0.5 || p.a>initialPlanetA*2) failure="planet_outside_retention_envelope";
            else if (maxEnergy>1e-4 || maxAngular>1e-8) failure="numerical_error_limit";
        };
        measure(true);
        for (; steps<count && failure=="none";) {
            if (!advanceAlienState(stars,planet,dt)) {failure="surface_contact";break;}
            time=++steps*dt;
            if (steps%checkStride==0 || steps==count) measure(steps%outputStride==0 || steps==count);
        }
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout << std::setprecision(17) << "{\"preset\":\"" << preset.name << "\",\"requested_years\":" << years
                  << ",\"time\":" << time << ",\"dt\":" << dt << ",\"phase_offset_deg\":" << phase << ",\"inclination_offset_deg\":" << inclinationOffset
                  << ",\"steps\":" << steps << ",\"checks\":" << checks << ",\"wall_seconds\":" << seconds
                  << ",\"passed\":" << (failure=="none" ? "true" : "false") << ",\"failure\":\"" << failure << "\",\"equal_masses\":" << (preset.equalMasses ? "true" : "false")
                  << ",\"initial_mutual_inclination_deg\":" << initialMutual << ",\"initial_planet_inclination_deg\":" << initialPlanetInclination
                  << ",\"initial_inner_a\":" << initialInnerA << ",\"initial_outer_a\":" << initialOuterA << ",\"initial_planet_a\":" << initialPlanetA
                  << ",\"max_star_span_AU\":" << maxSpan << ",\"min_star_separation_AU\":" << minStarSeparation
                  << ",\"max_stellar_energy_error\":" << maxEnergy << ",\"max_stellar_angular_error\":" << maxAngular
                  << ",\"max_stellar_momentum\":" << maxMomentum << ",\"max_barycentre_drift_AU\":" << maxBarycentre
                  << ",\"min_inner_a\":" << minInnerA << ",\"max_inner_a\":" << maxInnerA << ",\"max_inner_e\":" << maxInnerE
                  << ",\"min_outer_a\":" << minOuterA << ",\"max_outer_a\":" << maxOuterA << ",\"max_outer_e\":" << maxOuterE
                  << ",\"min_planet_a\":" << minPlanetA << ",\"max_planet_a\":" << maxPlanetA << ",\"max_planet_e\":" << maxPlanetE
                  << ",\"min_planet_distance_AU\":" << minPlanetDistance << ",\"max_planet_distance_AU\":" << maxPlanetDistance
                  << ",\"min_mutual_i_deg\":" << minMutual << ",\"max_mutual_i_deg\":" << maxMutual
                  << ",\"min_planet_i_deg\":" << minPlanetI << ",\"max_planet_i_deg\":" << maxPlanetI
                  << ",\"min_flux_Earth\":" << minFlux << ",\"max_flux_Earth\":" << maxFlux << ",\"initial_bodies\":[";
        for (std::size_t i=0;i<4;++i) {
            const auto& body=i<3 ? initialStarsState[i] : initialPlanet;
            if (i) std::cout << ',';
            std::cout << "{\"mass\":" << body.mass << ",\"position\":[" << body.position.x << ',' << body.position.y << ',' << body.position.z
                      << "],\"velocity\":[" << body.velocity.x << ',' << body.velocity.y << ',' << body.velocity.z << "]}";
        }
        std::cout << "]}\n";
        return 0;
    } catch (const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
}
