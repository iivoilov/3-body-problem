#include "alien_catalog.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <vector>
using namespace orbit;
namespace fs=std::filesystem;
namespace {
void scalar(std::ostream& out,double x) {
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
    const float f=static_cast<float>(x);std::uint32_t bits;std::memcpy(&bits,&f,4);
    const char bytes[]{static_cast<char>(bits),static_cast<char>(bits>>8),static_cast<char>(bits>>16),static_cast<char>(bits>>24)};
    out.write(bytes,4);
}
void vector(std::ostream& out,Vec3d v) {out<<'['<<v.x<<','<<v.y<<','<<v.z<<']';}
void frame(std::ostream& out,const CatalogSimulation& s) {
    for (std::size_t i=0;i<4;++i) {const auto v=i<3?s.stars[i].position:s.planet.position;scalar(out,v.x);scalar(out,v.y);scalar(out,v.z);}
    const auto m=s.measure();for(double x:{m.flux,m.nearest,m.separation,m.energyError,m.angularError}) scalar(out,x);
    for(double x:m.contributions) scalar(out,x);
}
void manifest(const fs::path& directory,const AlienPreset& p,double start,double duration,double interval,bool survey) {
    fs::create_directories(directory);
    std::ofstream out(directory/"manifest.json");
    out<<std::setprecision(17)<<"{\n\"format\":\"orbit-02-trajectory\",\"version\":1,\"encoding\":\"float32-le\",\"file\":\"world.bin\","
       <<"\"orbitId\":\""<<p.id<<"\",\"title\":\""<<p.title<<"\",\"fieldsPerFrame\":20,\"frameCount\":"<<std::llround(duration/interval)+1
       <<",\"startTime\":"<<start<<",\"duration\":"<<duration<<",\"sampleInterval\":"<<interval<<",\"interpolate\":"<<(survey?"false":"true")
       <<",\"kind\":\""<<(survey?"survey":"detail")<<"\",\"physicsStep\":"<<p.step<<",\"playbackScale\":"<<(survey?10000:p.playback)
       <<",\"integrator\":\""<<(p.fourthOrder?"Yoshida4 composition":"kick-drift-kick")<<"\",\"G\":"<<stellarGravity
       <<",\"rotationPeriod\":"<<p.rotation<<",\"skyPhase\":"<<p.skyPhase<<",\"surfaceYaw\":"<<catalogYaw(CatalogSimulation(p.id))
       <<",\"observerAmplitude\":"<<p.observerAmplitude<<",\"rotationDirection\":"<<p.rotationDirection
       <<",\"units\":{\"length\":\"AU\",\"mass\":\"solar mass\",\"time\":\"Julian year\",\"flux\":\"Earth incident flux\"},"
       <<"\"description\":\"Computed positions and native telemetry. Survey records are discrete snapshots; gaps are not interpolated. See dedicated orbit notes for discovery, tuning, validation, and limitations.\",\"stars\":[";
    for (int i=0;i<3;++i) {
        if(i) out<<',';
        out<<"{\"name\":\""<<p.names[i]<<"\",\"mass\":"<<p.stars[i].mass<<",\"radius\":"<<p.radii[i]<<",\"luminosity\":"<<p.luminosities[i]
           <<",\"displayRadius\":"<<p.displayRadii[i]<<",\"color\":["<<p.colors[i].x<<','<<p.colors[i].y<<','<<p.colors[i].z<<"],\"position\":";
        vector(out,p.stars[i].position);out<<",\"velocity\":";vector(out,p.stars[i].velocity);out<<'}';
    }
    out<<"],\"planet\":{\"name\":\"Trisolaris\",\"mass\":0,\"testParticle\":true,\"radius\":"<<alienPlanetRadius<<",\"displayRadius\":0.13,\"position\":";
    vector(out,p.planet.position);out<<",\"velocity\":";vector(out,p.planet.velocity);out<<"}}\n";
    if(!out) throw std::runtime_error("Cannot write manifest.");
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2 && argc!=4) throw std::runtime_error("Usage: orbit_catalog_export DOC_DATA_ROOT [--years INTEGER]");
        unsigned years=1000000;
        if(argc==4) {if(std::string(argv[2])!="--years") throw std::runtime_error("Unknown option.");years=std::stoul(argv[3]);}
        const unsigned surveyInterval=years>=10000?100:1;
        if(years<64 || years>1000000 || years%surveyInterval) throw std::runtime_error("Duration must be >=64 and divisible by survey cadence.");
        const fs::path root(argv[1]);fs::create_directories(root/"orbit-02");
        std::ofstream catalog(root/"orbit-02/catalog.json");
        catalog<<"{\"version\":1,\"orbits\":[{\"id\":\"02-01\",\"title\":\"The scattering world\",\"notes\":\"orbits/Orbit-02-01.md\",\"demo\":\"assets/orbit-02-01.gif\",\"records\":[{\"label\":\"Opening 36 years\",\"manifest\":\"data/orbit-02/manifest.json\"}]}";
        for(const std::string id:{"02-02","02-03"}) {
            CatalogSimulation s(id);const auto& p=s.preset;
            const fs::path directory=root/("orbit-"+id);
            std::set<unsigned> starts{0,std::min(years-64,years/4),std::min(years-64,years/2),std::min(years-64,years*3/4),years-64};
            struct Window {std::uint64_t start,end;std::ofstream out;};
            std::vector<Window> windows;
            catalog<<",{\"id\":\""<<id<<"\",\"title\":\""<<p.title<<"\",\"notes\":\"orbits/Orbit-"<<id<<".md\",\"demo\":\"assets/orbit-"<<id<<".gif\",\"records\":[";
            for(unsigned start:starts) {
                const auto path=directory/("year-"+std::to_string(start));
                manifest(path,p,start,64,1.0/128,false);
                if(!windows.empty()) catalog<<',';
                catalog<<"{\"label\":\"Years "<<start<<"–"<<start+64<<" · detail\",\"manifest\":\"data/orbit-"<<id<<"/year-"<<start<<"/manifest.json\"}";
                windows.push_back({static_cast<std::uint64_t>(std::llround(start/p.step)),static_cast<std::uint64_t>(std::llround((start+64)/p.step)),std::ofstream(path/"world.bin",std::ios::binary)});
                if(!windows.back().out) throw std::runtime_error("Cannot open detail record.");
            }
            const auto surveyPath=directory/"survey";
            manifest(surveyPath,p,0,years,surveyInterval,true);
            std::ofstream survey(surveyPath/"world.bin",std::ios::binary);
            if(!survey) throw std::runtime_error("Cannot open survey record.");
            catalog<<",{\"label\":\""<<years<<"-year survey · snapshots\",\"manifest\":\"data/orbit-"<<id<<"/survey/manifest.json\"}]}";
            const auto count=static_cast<std::uint64_t>(std::llround(years/p.step));
            const auto detailStride=static_cast<std::uint64_t>(std::llround((1.0/128)/p.step));
            const auto surveyStride=static_cast<std::uint64_t>(std::llround(surveyInterval/p.step));
            const auto checkStride=static_cast<std::uint64_t>(std::llround(.0625/p.step));
            double maxError=0,maxAngular=0,maxSpan=0,minSeparation=1e30,minFlux=1e30,maxFlux=0,minPlanet=1e30,maxPlanet=0;
            for(std::uint64_t step=0;step<=count;++step) {
                if(step%detailStride==0) for(auto& w:windows) if(step>=w.start && step<=w.end) frame(w.out,s);
                if(step%surveyStride==0) frame(survey,s);
                if(step%checkStride==0 || step==count) {
                    const auto m=s.measure();
                    maxError=std::max(maxError,std::abs(m.energyError));maxAngular=std::max(maxAngular,m.angularError);
                    minSeparation=std::min(minSeparation,m.separation);minFlux=std::min(minFlux,m.flux);maxFlux=std::max(maxFlux,m.flux);
                    for(int i=0;i<3;++i) for(int j=i+1;j<3;++j) maxSpan=std::max(maxSpan,length(s.stars[i].position-s.stars[j].position));
                    const auto center=id=="02-02"?centerOfMass(s.stars):s.stars[0].position;
                    const auto velocity=id=="02-02"?momentum(s.stars)*(1.0/3):s.stars[0].velocity;
                    const double mass=id=="02-02"?3:s.stars[0].mass;
                    const double r=length(s.planet.position-center);
                    minPlanet=std::min(minPlanet,r);maxPlanet=std::max(maxPlanet,r);
                    if(!std::isfinite(m.flux) || maxSpan>100 || r>(id=="02-02"?100:5) ||
                       stabilityOrbit(s.planet.position-center,s.planet.velocity-velocity,mass).specificEnergy>=0 || maxError>1e-4 || maxAngular>1e-8)
                        throw std::runtime_error(id+" failed retention/conservation at year "+std::to_string(s.time));
                }
                if(step<count && !s.step(false)) throw std::runtime_error(id+" reached surface contact.");
            }
            std::ofstream result(directory/"verification.json");
            result<<std::setprecision(17)<<"{\"id\":\""<<id<<"\",\"integratedYears\":"<<s.time<<",\"steps\":"<<count<<",\"passed\":true,\"checkInterval\":0.0625,\"maxStellarEnergyError\":"<<maxError
                  <<",\"maxStellarAngularError\":"<<maxAngular<<",\"maxStellarSpanAU\":"<<maxSpan<<",\"minStellarSeparationAU\":"<<minSeparation
                  <<",\"minPlanetDistanceAU\":"<<minPlanet<<",\"maxPlanetDistanceAU\":"<<maxPlanet<<",\"minFluxEarth\":"<<minFlux<<",\"maxFluxEarth\":"<<maxFlux<<"}\n";
            if(!survey || !result) throw std::runtime_error("Failed writing long record.");
            for(auto& w:windows) if(!w.out) throw std::runtime_error("Failed writing detail record.");
            std::cout<<id<<": integrated "<<s.time<<" years; "<<windows.size()<<" dense windows plus survey.\n"<<std::flush;
        }
        catalog<<"]}\n";
        if(!catalog) throw std::runtime_error("Failed writing catalogue.");
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
