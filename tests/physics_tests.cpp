#include "scene.hpp"
#include <cstdlib>
#include <iostream>

using namespace orbit;
namespace {
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(EXIT_FAILURE); }
}
void sameState(const Simulation& a, const Simulation& b, const char* description) {
    check(a.time == b.time, description);
    for (std::size_t i = 0; i < a.bodies.size(); ++i)
        check(length(a.bodies[i].position-b.bodies[i].position) < 1e-12 &&
              length(a.bodies[i].velocity-b.bodies[i].velocity) < 1e-12, description);
}
Bodies binary() {
    const double m0 = sunMass, m1 = planets[0].mass, r = planets[0].orbitRadius;
    const double v = std::sqrt(gravity * (m0+m1) / r);
    return {{{m0, {-r*m1/(m0+m1),0,0}, {0,0,-v*m1/(m0+m1)}},
             {m1, {r*m0/(m0+m1),0,0}, {0,0,v*m0/(m0+m1)}},
             {0, {100,0,0}, {}}}};
}
double binaryOrbitError(int steps) {
    Bodies bodies = binary();
    const Vec3d start = bodies[1].position - bodies[0].position;
    const double dt = initialPeriod(planets[0]) / steps;
    for (int i = 0; i < steps; ++i) leapfrogStep(bodies, dt);
    return length((bodies[1].position - bodies[0].position) - start);
}
}

int main() {
    const Bodies initial = initialBodies();
    check(length(centerOfMass(initial)) < 1e-14, "initial barycentre is at the origin");
    check(length(momentum(initial)) < 1e-13, "initial total momentum is zero");
    for (std::size_t i = 0; i < planets.size(); ++i) {
        const Vec3d r = initial[i+1].position - initial[0].position;
        const Vec3d v = initial[i+1].velocity - initial[0].velocity;
        check(std::abs(length(r)-planets[i].orbitRadius) < 1e-12, "initial distances match the configured preset");
        check(std::abs(dot(r,v)) < 1e-12, "initial velocity is tangential");
        check(std::abs(dot(v,v)-gravity*(sunMass+planets[i].mass)/length(r)) < 1e-12,
              "initial speed is the two-body circular speed");
    }
    check(std::abs(initialPeriod(planets[0])-24) < 0.1, "inner initial period stays close to 24 simulated seconds");
    check(std::abs(initialPeriod(planets[1])/initialPeriod(planets[0])-2.05) < 1e-6,
          "initial outer/inner period ratio is 2.05");

    const auto acceleration = accelerations(initial);
    Vec3d totalForce;
    for (std::size_t i = 0; i < 3; ++i) totalForce = totalForce + acceleration[i]*initial[i].mass;
    check(length(totalForce) < 1e-14, "pair forces conserve linear momentum");
    check(length(acceleration[0]) > 1e-6, "sun responds to the planets");
    Bodies doubledDistance = binary();
    const auto binaryAcceleration = accelerations(doubledDistance);
    doubledDistance[1].position = doubledDistance[0].position
                               + (doubledDistance[1].position-doubledDistance[0].position)*2;
    check(length(accelerations(doubledDistance)[0]-binaryAcceleration[0]*0.25) < 1e-14,
          "doubling separation reduces gravity to one quarter");
    Bodies noOuterMass = initial;
    noOuterMass[2].mass = 0;
    check(length(acceleration[1]-accelerations(noOuterMass)[1]) > 1e-5, "inner planet feels outer planet gravity");
    Bodies translated = initial;
    for (auto& body : translated) body.position = body.position + Vec3d{50,20,-30};
    const auto translatedAcceleration = accelerations(translated);
    for (std::size_t i = 0; i < 3; ++i)
        check(length(acceleration[i]-translatedAcceleration[i]) < 1e-13, "gravity depends only on relative positions");

    Simulation whole, partitioned;
    whole.advance(8);
    for (int i = 0; i < 1024; ++i) partitioned.advance(8.0/1024);
    sameState(whole, partitioned, "physics is independent of render frame partitioning");
    check(length(whole.bodies[0].position-initial[0].position) > 0.001, "sun actually moves");
    partitioned.speed = 1;
    partitioned.reset();
    partitioned.speed = 2;
    partitioned.advance(4);
    sameState(whole, partitioned, "playback speed does not change the physics at equal simulated time");
    Simulation oneSecond;
    oneSecond.advance(1);
    check(oneSecond.speed == 1 && oneSecond.time == 4, "displayed 1x is four simulated seconds per real second");
    const Bodies beforePause = whole.bodies;
    whole.paused = true;
    whole.advance(20);
    check(length(whole.bodies[1].position-beforePause[1].position) == 0, "pause freezes positions");
    const double beforeStep = whole.time;
    whole.singleStep();
    check(whole.paused && whole.time == beforeStep+0.125, "single step advances 1/8 simulated second and stays paused");

    const double coarse = binaryOrbitError(256), fine = binaryOrbitError(512);
    check(fine < coarse*0.3 && fine > coarse*0.2, "halving the step gives second-order orbital convergence");
    Bodies reversible = binary();
    const Bodies original = reversible;
    for (int i = 0; i < 2000; ++i) leapfrogStep(reversible, physicsStep);
    for (int i = 0; i < 2000; ++i) leapfrogStep(reversible, -physicsStep);
    for (std::size_t i = 0; i < 2; ++i)
        check(length(reversible[i].position-original[i].position) < 1e-10 &&
              length(reversible[i].velocity-original[i].velocity) < 1e-10, "leapfrog is time reversible");

    Simulation longRun;
    double maxEnergyError = 0, minInner = 1e9, maxInner = 0;
    const int steps = static_cast<int>(100*initialPeriod(planets[0])/physicsStep);
    for (int i = 0; i < steps; ++i) {
        check(longRun.step(), "default system avoids contact during 100 initial inner periods");
        if (i % 256 == 0) {
            maxEnergyError = std::max(maxEnergyError, std::abs(longRun.energyError()));
            const double r = length(longRun.bodies[1].position-longRun.bodies[0].position);
            minInner = std::min(minInner, r); maxInner = std::max(maxInner, r);
        }
    }
    check(maxEnergyError < 1e-6, "energy error stays bounded over 100 initial inner periods");
    check(longRun.angularMomentumError() < 1e-10, "angular momentum is conserved");
    check(length(momentum(longRun.bodies)) < 1e-10, "linear momentum is conserved");
    check(length(centerOfMass(longRun.bodies)) < 1e-9, "barycentre does not drift");
    check(maxInner-minInner > 0.01, "planet paths evolve rather than remaining forced circles");
    for (const auto& body : longRun.bodies)
        check(body.position.y == 0 && body.velocity.y == 0, "planar initial conditions remain planar");
    check(longRun.trails[0].size() == 2048 && longRun.trails[1].size() == 2048, "trail history stays bounded");

    Simulation contact;
    contact.bodies[1].position = contact.bodies[0].position;
    check(!contact.step() && contact.paused && contact.collision, "contact pauses before a singular force evaluation");
    contact.reset();
    check(!contact.collision && !contact.paused && contact.time == 0 && contact.trails[0].size() == 1,
          "reset restores bodies, trails, and contact state");
    contact.bodies[1].position = contact.bodies[0].position + Vec3d{sunRadius+planets[0].radius+0.001,0,0};
    contact.bodies[1].velocity = {-1,0,0};
    const Vec3d beforeContact = contact.bodies[1].position;
    check(!contact.step() && contact.collision && contact.time == 0 &&
          length(contact.bodies[1].position-beforeContact) == 0,
          "a step that would cause contact pauses at the last valid state");

    std::cout << "Physics tests passed: initial net momentum " << length(momentum(initial))
              << "; 100 inner periods, max relative energy error " << maxEnergyError
              << ", relative angular-momentum error " << longRun.angularMomentumError()
              << ", barycentre drift " << length(centerOfMass(longRun.bodies))
              << ", inner distance range " << minInner << " to " << maxInner << ".\n";
}
