#include "alien_catalog.hpp"
#include <cstdlib>
#include <iostream>
using namespace orbit;
namespace {void check(bool value,const char* message) {if(!value) {std::cerr<<message<<'\n';std::exit(1);}}}
int main() {
    AlienSimulation old;
    CatalogSimulation current;
    for(int step=0;step<8192;++step) {
        check(old.step() && current.step(),"original fixture remains contact free");
        for(int i=0;i<3;++i) check(length(old.stars[i].position-current.stars[i].position)==0,"02-01 preserves original stellar trajectory");
        check(length(old.planet.position-current.planet.position)==0,"02-01 preserves original planetary trajectory");
    }
    for(const auto id:alienPresetIds) {
        CatalogSimulation whole(id),parts(id);
        whole.advance(2);
        for(int i=0;i<256;++i) parts.advance(2.0/256);
        check(whole.time==parts.time && length(whole.planet.position-parts.planet.position)==0,"frame partition does not affect numbered worlds");
        whole.reset();check(whole.preset.id==id && whole.time==0,"reset retains orbit identity");
        check(length(centerOfMass(whole.stars))<1e-13 && length(momentum(whole.stars))<1e-13,"each orbit starts barycentric");
    }
    CatalogSimulation dance("02-02");
    for(const auto& star:dance.stars) check(star.mass==1,"spatial dance has equal stellar masses");
    dance.planet.position=dance.stars[0].position+Vec3d{.0052,0,0};
    check(!catalogContact(dance.preset,dance.stars,dance.planet),"catalogue uses this preset's solar radius");
    dance.planet.position=dance.stars[0].position;
    check(!dance.step() && dance.paused && dance.collision && dance.time==0,"contact stops before singular evaluation");
    const auto expected=stabilityInitialState(stabilityPresets[3]);
    CatalogSimulation hierarchy("02-03");
    check(length(expected.second.position-hierarchy.planet.position)==0,"02-03 uses validated inclined-16 state");
    bool rejected=false;try {CatalogSimulation unknown("02-99");}catch(const std::runtime_error&) {rejected=true;}
    check(rejected,"unknown orbit fails explicitly");
    std::cout<<"Catalogue physics: legacy equality, frame independence, identity, masses, contact, and validated initial states passed.\n";
}
