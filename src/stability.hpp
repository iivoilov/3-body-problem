#pragma once

#include "alien_world.hpp"
#include <utility>

namespace orbit {
struct StabilityPreset {
    const char* name;
    double innerA = 4, innerE = 0.05, outerA = 24, outerE = 0.10;
    double outerInclination = 20, planetA = 0.90, planetE = 0.03, planetInclination = 10;
    double outerPhase = 75, planetPhase = 225;
    bool equalMasses = false, current = false;
};
inline const std::array<StabilityPreset,8> stabilityPresets{{
    {"current",4,0,6,0,0,0.9,0.16,0,75,225,false,true},
    {"coplanar-24",4,0.05,24,0.1,0,0.9,0.03,0},
    {"inclined-12",4,0.05,12,0.1,20,0.9,0.03,10},
    {"inclined-16",4,0.05,16,0.1,20,0.9,0.03,10},
    {"inclined-24",4,0.05,24,0.1,20,0.9,0.03,10},
    {"inclined-32",4,0.05,32,0.1,20,0.9,0.03,10},
    {"high-inclination-24",4,0.05,24,0.1,60,0.9,0.03,10},
    {"solar-triplet-24",4,0.05,24,0.1,20,0.75,0.03,10,75,225,true}
}};

struct RelativeState { Vec3d position, velocity; };
inline RelativeState keplerState(double mass, double a, double e, double inclination,
                                 double node, double periapsis, double anomaly) {
    const double radians = physicsPi/180;
    const double i=inclination*radians, o=node*radians, w=periapsis*radians, f=anomaly*radians;
    const Vec3d p{std::cos(o)*std::cos(w)-std::sin(o)*std::sin(w)*std::cos(i),
                 std::sin(w)*std::sin(i),
                 std::sin(o)*std::cos(w)+std::cos(o)*std::sin(w)*std::cos(i)};
    const Vec3d q{-std::cos(o)*std::sin(w)-std::sin(o)*std::cos(w)*std::cos(i),
                 std::cos(w)*std::sin(i),
                 -std::sin(o)*std::sin(w)+std::cos(o)*std::cos(w)*std::cos(i)};
    const double r=a*(1-e*e)/(1+e*std::cos(f));
    const double speed=std::sqrt(stellarGravity*mass/(a*(1-e*e)));
    return {(p*std::cos(f)+q*std::sin(f))*r,
            (p*(-std::sin(f))+q*(e+std::cos(f)))*speed};
}

inline std::pair<Bodies,Body> stabilityInitialState(const StabilityPreset& preset) {
    if (preset.current) { auto stars=initialStars(); return {stars,initialAlienPlanet(stars)}; }
    const auto masses=preset.equalMasses ? std::array<double,3>{{1,1,1}} : stellarMasses;
    const double innerMass=masses[0]+masses[1], totalMass=innerMass+masses[2];
    const auto inner=keplerState(innerMass,preset.innerA,preset.innerE,0,0,0,0);
    const auto outer=keplerState(totalMass,preset.outerA,preset.outerE,preset.outerInclination,35,40,preset.outerPhase);
    const Vec3d center=outer.position*(-masses[2]/totalMass);
    const Vec3d velocity=outer.velocity*(-masses[2]/totalMass);
    Bodies stars{{
        {masses[0],center-inner.position*(masses[1]/innerMass),velocity-inner.velocity*(masses[1]/innerMass)},
        {masses[1],center+inner.position*(masses[0]/innerMass),velocity+inner.velocity*(masses[0]/innerMass)},
        {masses[2],outer.position*(innerMass/totalMass),outer.velocity*(innerMass/totalMass)}
    }};
    const auto planet=keplerState(masses[0],preset.planetA,preset.planetE,preset.planetInclination,45,25,preset.planetPhase);
    return {stars,{0,stars[0].position+planet.position,stars[0].velocity+planet.velocity}};
}

struct StabilityOrbit {
    double a=0, eccentricity=0, specificEnergy=0;
    Vec3d angular;
};
inline StabilityOrbit stabilityOrbit(Vec3d r, Vec3d v, double mass) {
    StabilityOrbit result;
    const double mu=stellarGravity*mass, radius=length(r);
    result.specificEnergy=dot(v,v)*0.5-mu/radius;
    result.a=-mu/(2*result.specificEnergy);
    result.angular=cross(r,v);
    result.eccentricity=length(cross(v,result.angular)*(1/mu)-r*(1/radius));
    return result;
}
inline double mutualInclination(Vec3d a, Vec3d b) {
    return std::acos(std::clamp(dot(a,b)/(length(a)*length(b)),-1.0,1.0))*180/physicsPi;
}
inline StabilityOrbit innerStellarOrbit(const Bodies& stars) {
    return stabilityOrbit(stars[1].position-stars[0].position,stars[1].velocity-stars[0].velocity,
                          stars[0].mass+stars[1].mass);
}
inline StabilityOrbit outerStellarOrbit(const Bodies& stars) {
    const double m=stars[0].mass+stars[1].mass;
    const Vec3d center=(stars[0].position*stars[0].mass+stars[1].position*stars[1].mass)*(1/m);
    const Vec3d velocity=(stars[0].velocity*stars[0].mass+stars[1].velocity*stars[1].mass)*(1/m);
    return stabilityOrbit(stars[2].position-center,stars[2].velocity-velocity,m+stars[2].mass);
}
} // namespace orbit
