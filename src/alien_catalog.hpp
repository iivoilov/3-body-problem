#pragma once
#include "stability.hpp"
#include <stdexcept>
#include <string>

namespace orbit {
struct AlienPreset {
    std::string id="02-01", title="THE SCATTERING WORLD";
    Bodies stars=initialStars();
    Body planet=initialAlienPlanet(stars);
    std::array<double,3> luminosities=stellarLuminosities, radii=stellarRadii;
    std::array<Vec3,3> colors=stellarColors;
    std::array<float,3> displayRadii=stellarDisplayRadii;
    std::array<const char*,3> names=stellarNames;
    double step=alienStep, playback=alienPlayback, rotation=alienRotationPeriod, skyPhase=0.57;
    double observerAmplitude=.28, rotationDirection=1;
    unsigned recordStride=64;
    bool fourthOrder=false;
};
inline const std::array<const char*,3> alienPresetIds{{"02-01","02-02","02-03"}};
inline AlienPreset alienPreset(const std::string& id) {
    AlienPreset p;
    if (id=="02-01") return p;
    p.id=id; p.step=1.0/512; p.recordStride=8;
    if (id=="02-02") {
        p.title="THE SPATIAL SUN DANCE";
        // Li & Liao, arXiv:2508.08568v1, Table S4. Same initial state as the research executable.
        constexpr double z=.4768782642803115460616110933545915887541644925960260369253641303299294;
        constexpr double vx=.4021369100747237493832253390929016257978052115111599603699588130212840;
        constexpr double vy=.1803569512862586641317199478891917026060004921964179954624832790547685;
        constexpr double vz=.2104451281378731221497174690764795110575771159319962594676582115390083;
        const double speed=std::sqrt(stellarGravity/4);
        p.stars={{{1,{-4,0,0},{vx*speed,vz*speed,vy*speed}},
                  {1,{4,0,0},{vx*speed,-vz*speed,vy*speed}},
                  {1,{0,z*4,0},{-2*vx*speed,0,-2*vy*speed}}}};
        const auto center=centerOfMass(p.stars);
        for (auto& star:p.stars) star.position=star.position-center;
        const auto planet=keplerState(3,20,0,20,0,0,0);
        p.planet={0,planet.position,planet.velocity};
        p.luminosities={1,1,1};
        p.radii={0.00465047,0.00465047,0.00465047};
        p.displayRadii={0.30f,0.30f,0.30f};
        p.colors={{{1,.80f,.48f},{1,.43f,.16f},{.72f,.86f,1}}};
        p.names={"SUN A","SUN B","SUN C"};
        p.fourthOrder=true; p.playback=2; p.rotation=std::sqrt(20*20*20/3.0);
        p.skyPhase=1.5*physicsPi; p.observerAmplitude=.5; p.rotationDirection=-1;
    } else if (id=="02-03") {
        p.title="THE INCLINED HIERARCHY";
        const auto initial=stabilityInitialState(stabilityPresets[3]); // Validated inclined-16.
        p.stars=initial.first; p.planet=initial.second;
        p.playback=.8; p.rotation=8;
    } else throw std::runtime_error("Unknown alien orbit: "+id);
    return p;
}
inline bool catalogContact(const AlienPreset& p,const Bodies& stars,const Body& planet) {
    for (std::size_t i=0;i<3;++i) {
        if (length(stars[i].position-planet.position)<=p.radii[i]+alienPlanetRadius) return true;
        for (std::size_t j=i+1;j<3;++j)
            if (length(stars[i].position-stars[j].position)<=p.radii[i]+p.radii[j]) return true;
    }
    return false;
}
inline bool catalogLeapfrog(const AlienPreset& p,Bodies& stars,Body& planet,double dt) {
    if (catalogContact(p,stars,planet)) return false;
    Bodies next=stars;Body world=planet;
    kick(next,dt*.5*stellarGravity/gravity);
    world.velocity=world.velocity+planetAcceleration(stars,planet.position)*(dt*.5);
    drift(next,dt);world.position=world.position+world.velocity*dt;
    if (catalogContact(p,next,world)) return false;
    kick(next,dt*.5*stellarGravity/gravity);
    world.velocity=world.velocity+planetAcceleration(next,world.position)*(dt*.5);
    stars=next;planet=world;return true;
}
inline bool advanceCatalogState(const AlienPreset& p,Bodies& stars,Body& planet) {
    if (p.fourthOrder) {
        constexpr double a=1.3512071919596576340,b=-1.7024143839193152681;
        for (double weight:{a,b,a}) if (!catalogLeapfrog(p,stars,planet,p.step*weight)) return false;
        return true;
    }
    return catalogLeapfrog(p,stars,planet,p.step);
}
class CatalogSimulation {
public:
    AlienPreset preset;
    Bodies stars;
    Body planet;
    std::array<std::deque<Vec3>,4> trails;
    std::deque<AlienSample> history;
    double time=0,speed=1;
    bool paused=false,collision=false;
    explicit CatalogSimulation(const std::string& id="02-01") : preset(alienPreset(id)),stars(preset.stars),planet(preset.planet),
        initialEnergy_(stellarEnergy(stars)),initialAngular_(angularMomentum(stars)) {record();}
    bool step(bool recordHistory=true) {
        if (collision || !advanceCatalogState(preset,stars,planet)) {collision=paused=true;return false;}
        time=++steps_*preset.step;
        if (recordHistory && steps_%preset.recordStride==0) record();
        return true;
    }
    void advance(double seconds) {if (!paused && seconds>0) integrate(seconds*preset.playback*speed);}
    void singleStep() {if (paused && !collision) integrate(16*preset.step);}
    void faster() {speed=std::min(16.0,speed*2);}
    void slower() {speed=std::max(1.0/64,speed*.5);}
    void reset() {*this=CatalogSimulation(preset.id);}
    AlienSample measure() const {
        AlienSample s; s.time=time; s.nearest=s.separation=1e30;
        for (std::size_t i=0;i<3;++i) {
            const double distance=length(stars[i].position-planet.position);
            s.contributions[i]=preset.luminosities[i]/(distance*distance);
            s.flux+=s.contributions[i];s.nearest=std::min(s.nearest,distance);
            for (std::size_t j=i+1;j<3;++j) s.separation=std::min(s.separation,length(stars[i].position-stars[j].position));
        }
        s.energyError=(stellarEnergy(stars)-initialEnergy_)/std::abs(initialEnergy_);
        s.angularError=length(angularMomentum(stars)-initialAngular_)/length(initialAngular_);
        return s;
    }
private:
    std::uint64_t steps_=0;
    double pending_=0,initialEnergy_;
    Vec3d initialAngular_;
    void integrate(double duration) {
        pending_+=duration;
        while (pending_+1e-14>=preset.step) {
            if (!step()) {pending_=0;return;}
            pending_=std::max(0.0,pending_-preset.step);
        }
    }
    void record() {
        for (std::size_t i=0;i<4;++i) {
            trails[i].push_back((i<3?stars[i].position:planet.position).asFloat());
            if (trails[i].size()>2048) trails[i].pop_front();
        }
        history.push_back(measure());if (history.size()>2048) history.pop_front();
    }
};
inline std::array<Vec3,3> catalogSkyDirections(const CatalogSimulation& s) {
    if (s.preset.id=="02-01") return alienSkyDirections(s.stars,s.planet,s.time);
    const double angle=s.preset.skyPhase+s.preset.rotationDirection*2*physicsPi*std::fmod(s.time,s.preset.rotation)/s.preset.rotation;
    const float amplitude=static_cast<float>(s.preset.observerAmplitude);
    const Vec3 up{static_cast<float>(amplitude*std::sin(angle)),std::sqrt(1-amplitude*amplitude),static_cast<float>(amplitude*std::cos(angle))};
    const Vec3 north=normalized(Vec3{0,1,0}-up*up.y),east=normalized(cross(north,up));
    std::array<Vec3,3> result;
    for(int i=0;i<3;++i) {
        const auto direction=normalized((s.stars[i].position-s.planet.position).asFloat());
        result[i]={dot(direction,east),dot(direction,up),dot(direction,north)};
    }
    return result;
}
inline float catalogYaw(const CatalogSimulation& s) {
    if (s.preset.id=="02-01") return 2.93f;
    const auto directions=catalogSkyDirections(s);
    Vec3 center;
    const auto sample=s.measure();
    for (std::size_t i=0;i<3;++i) center=center+directions[i]*static_cast<float>(sample.contributions[i]);
    return std::atan2(center.x,center.z);
}
} // namespace orbit
