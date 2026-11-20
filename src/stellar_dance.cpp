// Research-only nonhierarchical triple; does not change the interactive world.
#include "stability.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace orbit;
int main(int argc,char** argv) {
    try {
        double years=1000000,dt=1.0/512,scale=4,radius=4,phase=0,perturb=0;
        std::string path;
        for (int i=1;i<argc;i+=2) {
            if (i+1==argc) throw std::runtime_error("Missing option value.");
            const std::string option=argv[i];
            if (option=="--output") {path=argv[i+1];continue;}
            std::size_t consumed=0;
            const double value=std::stod(argv[i+1],&consumed);
            if (consumed!=std::string(argv[i+1]).size() || !std::isfinite(value)) throw std::runtime_error("Invalid number.");
            if (option=="--years") years=value;
            else if (option=="--step") dt=value;
            else if (option=="--scale") scale=value;
            else if (option=="--radius") radius=value;
            else if (option=="--phase") phase=value;
            else if (option=="--perturb") perturb=value;
            else throw std::runtime_error("Unknown option.");
        }
        if (years<=0 || dt<=0 || dt>0.01 || scale<=0 || radius<=0 || years/dt>1e12)
            throw std::runtime_error("Invalid duration, step, or scale.");
        // Li & Liao, arXiv:2508.08568v1, Table S4, O_3(1.0), G=1.
        // Map published (x,y,z) to project (x,z,y); restore G=4*pi^2.
        constexpr double z=.4768782642803115460616110933545915887541644925960260369253641303299294;
        constexpr double vx=.4021369100747237493832253390929016257978052115111599603699588130212840;
        constexpr double vy=.1803569512862586641317199478891917026060004921964179954624832790547685;
        constexpr double vz=.2104451281378731221497174690764795110575771159319962594676582115390083;
        constexpr double period=6.831622036284445040766025062746064085737914286107653347931784678966507;
        const double speed=std::sqrt(stellarGravity/scale);
        Bodies stars{{{1,{-scale,0,0},{vx*speed,vz*(1+perturb)*speed,vy*speed}},
                      {1,{scale,0,0},{vx*speed,-vz*(1+perturb)*speed,vy*speed}},
                      {1,{0,z*scale,0},{-2*vx*speed,0,-2*vy*speed}}}};
        const auto center=centerOfMass(stars);
        for (auto& star:stars) star.position=star.position-center;
        const auto relative=keplerState(3,radius*scale,0,20,0,0,phase);
        Body planet{0,relative.position,relative.velocity};
        const auto initialStarsState=stars;
        const auto initialPlanet=planet;
        const double initialEnergy=stellarEnergy(stars);
        const auto initialAngular=angularMomentum(stars);
        const double angularScale=length(initialAngular);
        const std::uint64_t count=static_cast<std::uint64_t>(std::ceil(years/dt));
        const std::uint64_t stride=std::max<std::uint64_t>(1,std::llround(0.0625/dt));
        const std::uint64_t recordStride=std::max<std::uint64_t>(1,std::llround(0.0625/dt));
        std::ofstream csv;
        if (!path.empty()) {
            csv.open(path);
            if (!csv) throw std::runtime_error("Cannot open CSV.");
            csv << "time,ax,ay,az,bx,by,bz,cx,cy,cz,px,py,pz,flux,star_span,planet_radius,energy_error\n" << std::setprecision(17);
        }
        double time=0,maxSpan=0,minSeparation=1e30,minRadius=1e30,maxRadius=0,minA=1e30,maxA=0,maxEcc=0;
        double maxEnergy=0,maxAngular=0,maxDrift=0,minFlux=1e30,maxFlux=0,minSkyAngle=180,maxSkyAngle=0,minStarDistance=1e30;
        std::string failure="none";
        std::uint64_t steps=0,checks=0;
        const auto start=std::chrono::steady_clock::now();
        auto measure=[&](bool record) {
            ++checks;
            const auto barycenter=centerOfMass(stars);
            const auto velocity=momentum(stars)*(1.0/3);
            const auto p=stabilityOrbit(planet.position-barycenter,planet.velocity-velocity,3);
            const double distance=length(planet.position-barycenter);
            const double error=(stellarEnergy(stars)-initialEnergy)/std::abs(initialEnergy);
            double span=0,flux=0;
            for (std::size_t i=0;i<3;++i) {
                const auto r=stars[i].position-planet.position;
                const double d=length(r);
                flux+=1/(d*d);
                minStarDistance=std::min(minStarDistance,d);
                for (std::size_t j=i+1;j<3;++j) {
                    const double separation=length(stars[i].position-stars[j].position);
                    span=std::max(span,separation);minSeparation=std::min(minSeparation,separation);
                    const auto other=stars[j].position-planet.position;
                    const double angle=std::acos(std::clamp(dot(r,other)/(d*length(other)),-1.0,1.0))*180/physicsPi;
                    minSkyAngle=std::min(minSkyAngle,angle);maxSkyAngle=std::max(maxSkyAngle,angle);
                }
            }
            maxSpan=std::max(maxSpan,span);minRadius=std::min(minRadius,distance);maxRadius=std::max(maxRadius,distance);
            minA=std::min(minA,p.a);maxA=std::max(maxA,p.a);maxEcc=std::max(maxEcc,p.eccentricity);
            maxEnergy=std::max(maxEnergy,std::abs(error));
            maxAngular=std::max(maxAngular,length(angularMomentum(stars)-initialAngular)/angularScale);
            maxDrift=std::max(maxDrift,length(barycenter));
            minFlux=std::min(minFlux,flux);maxFlux=std::max(maxFlux,flux);
            if (record && csv.is_open()) {
                csv << time;
                for (const auto& star:stars) csv << ',' << star.position.x << ',' << star.position.y << ',' << star.position.z;
                csv << ',' << planet.position.x << ',' << planet.position.y << ',' << planet.position.z
                    << ',' << flux << ',' << span << ',' << distance << ',' << error << '\n';
            }
            if (!std::isfinite(distance) || !std::isfinite(span) || !std::isfinite(p.a) || !std::isfinite(error)) failure="nonfinite_state";
            else if (span>100) failure="stars_exceed_100_AU";
            else if (p.specificEnergy>=0) failure="planet_osculating_energy_nonnegative";
            else if (distance>100) failure="planet_exceeds_100_AU";
            else if (maxEnergy>1e-4 || maxAngular>1e-8) failure="numerical_error_limit";
        };
        measure(true);
        for (;steps<count && failure=="none";) {
            // Fourth-order symmetric composition (Yoshida): reuse existing leapfrog.
            constexpr double w1=1.3512071919596576340,w0=-1.7024143839193152681;
            if (!advanceAlienState(stars,planet,w1*dt) || !advanceAlienState(stars,planet,w0*dt)
                || !advanceAlienState(stars,planet,w1*dt)) {failure="surface_contact";break;}
            time=++steps*dt;
            if (steps%stride==0 || steps==count)
                measure(time<=64 && steps%recordStride==0);
        }
        double returnError=0;
        for (std::size_t i=0;i<3;++i) returnError=std::max(returnError,length(stars[i].position-initialStarsState[i].position)/scale);
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout << std::setprecision(17) << "{\"family\":\"Li-Liao O3(1.0)\",\"scale_AU\":" << scale
            << ",\"planet_initial_radius_AU\":" << radius*scale << ",\"planet_phase_deg\":" << phase << ",\"stellar_velocity_perturbation\":" << perturb
            << ",\"reference_stellar_period_years\":" << period*std::sqrt(scale*scale*scale/stellarGravity)
            << ",\"integrator\":\"Yoshida fourth-order composition of existing leapfrog\",\"requested_years\":" << years << ",\"time\":" << time << ",\"dt\":" << dt << ",\"steps\":" << steps << ",\"checks\":" << checks
            << ",\"passed\":" << (failure=="none" ? "true" : "false") << ",\"failure\":\"" << failure << "\",\"wall_seconds\":" << seconds
            << ",\"max_star_span_AU\":" << maxSpan << ",\"min_star_separation_AU\":" << minSeparation
            << ",\"min_planet_radius_AU\":" << minRadius << ",\"max_planet_radius_AU\":" << maxRadius
            << ",\"min_planet_a_AU\":" << minA << ",\"max_planet_a_AU\":" << maxA << ",\"max_planet_e\":" << maxEcc
            << ",\"max_stellar_energy_error\":" << maxEnergy << ",\"max_stellar_angular_error\":" << maxAngular << ",\"max_barycentre_drift_AU\":" << maxDrift
            << ",\"min_flux_Earth\":" << minFlux << ",\"max_flux_Earth\":" << maxFlux
            << ",\"min_sky_separation_deg\":" << minSkyAngle << ",\"max_sky_separation_deg\":" << maxSkyAngle
            << ",\"min_planet_star_distance_AU\":" << minStarDistance << ",\"final_scaled_position_return_error\":" << returnError << ",\"initial_bodies\":[";
        auto writeBodies=[&](const Bodies& state,const Body& world) {
            for (std::size_t i=0;i<4;++i) {
                const auto& b=i<3 ? state[i] : world;
                if (i) std::cout << ',';
                std::cout << "{\"mass\":" << b.mass << ",\"position\":[" << b.position.x << ',' << b.position.y << ',' << b.position.z
                          << "],\"velocity\":[" << b.velocity.x << ',' << b.velocity.y << ',' << b.velocity.z << "]}";
            }
        };
        writeBodies(initialStarsState,initialPlanet);
        std::cout << "],\"final_bodies\":[";
        writeBodies(stars,planet);
        std::cout << "]}\n";
    } catch (const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
}
