#include "scene.hpp"
#include <cstdlib>
#include <iostream>

using namespace orbit;
namespace {
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(EXIT_FAILURE); }
}
bool near(double a, double b, double tolerance = 1e-10) { return std::abs(a-b) < tolerance; }

// An analytic elliptic two-body state, with a known eccentric anomaly and periapsis.
Body ellipse(const Body& sun, double mass, double a, double e, double periapsis, double anomaly) {
    const double c = std::cos(anomaly), s = std::sin(anomaly), beta = std::sqrt(1-e*e);
    const double radius = a*(1-e*c), factor = std::sqrt(gravity*(sun.mass+mass)*a)/radius;
    const Vec3d position{a*(c-e), 0, a*beta*s}, velocity{-factor*s, 0, factor*beta*c};
    const auto rotate = [periapsis](Vec3d p) {
        return Vec3d{p.x*std::cos(periapsis)-p.z*std::sin(periapsis), p.y,
                     p.x*std::sin(periapsis)+p.z*std::cos(periapsis)};
    };
    return {mass, sun.position+rotate(position), sun.velocity+rotate(velocity)};
}
double eccentricAnomaly(double meanAnomaly, double eccentricity) {
    double anomaly = wrapAngle(meanAnomaly);
    for (int i = 0; i < 12; ++i)
        anomaly -= (anomaly-eccentricity*std::sin(anomaly)-wrapAngle(meanAnomaly)) /
                   (1-eccentricity*std::cos(anomaly));
    return anomaly;
}

// Independent Kepler orbits are fixtures for the measurement, not a claim that this pair is resonant.
Bodies fixture(double time, double outerDistance) {
    const Body sun{sunMass, {10,3,-4}, {0.2,-0.1,0.3}};
    Bodies bodies{{sun, {}, {}}};
    for (int i = 0; i < 2; ++i) {
        const double a = i == 0 ? 5 : outerDistance;
        const double e = i == 0 ? 0.1 : 0.15, periapsis = i == 0 ? 0.2 : 0.3;
        const double n = std::sqrt(gravity*(sunMass+planets[i].mass)/(a*a*a));
        const double meanLongitude = n*time + (i == 0 ? 0.1 : 0.4);
        bodies[i+1] = ellipse(sun, planets[i].mass, a, e, periapsis, eccentricAnomaly(meanLongitude-periapsis, e));
    }
    return bodies;
}
}

int main() {
    const Body sun{sunMass, {10,3,-4}, {0.2,-0.1,0.3}};
    for (double anomaly : {0.0, 0.5, 2.1, 3.5, 5.9}) {
        const Body planet = ellipse(sun, 2, 5, 0.2, 0.6, anomaly);
        const auto elements = orbitalElements(sun, planet);
        check(elements.bound && elements.phaseReliable, "elliptic planar orbit has reliable elements");
        check(near(elements.semimajorAxis, 5) && near(elements.eccentricity, 0.2), "recover analytic ellipse size and shape");
        check(near(wrapAngle(elements.periapsis-0.6), 0), "recover ellipse orientation");
        check(near(wrapAngle(elements.meanLongitude-(0.6+anomaly-0.2*std::sin(anomaly))), 0),
              "recover mean longitude through all anomaly quadrants");
        check(near(elements.period, initialPeriod(planets[0])), "recover Kepler period independently of orbital phase");
    }
    const Body circular = ellipse(sun, 2, 5, 0, 0.6, 1.2);
    const auto circle = orbitalElements(sun, circular);
    check(circle.bound && circle.eccentricity < 1e-12 && !circle.phaseReliable, "circular orbit has no reliable periapsis angle");
    check(near(wrapAngle(circle.meanLongitude-1.8), 0), "mean longitude remains defined for a circle");
    Body unbound = circular;
    unbound.velocity = sun.velocity + (circular.velocity-sun.velocity)*2;
    check(!orbitalElements(sun, unbound).bound, "unbound body has no finite elliptic period");
    Body retrograde = ellipse(sun, 2, 5, 0.2, 0.6, 1.2);
    retrograde.velocity = sun.velocity-(retrograde.velocity-sun.velocity);
    check(!orbitalElements(sun, retrograde).phaseReliable, "prograde phase diagnostic rejects retrograde motion");

    PhaseTracker wrapping;
    const double beforeWrap = wrapping.update(physicsPi-0.1, true);
    const double afterWrap = wrapping.update(-physicsPi+0.1, true);
    check(near(afterWrap-beforeWrap, 0.2/(2*physicsPi)), "phase unwrapping crosses the branch cut continuously");
    const auto segment = wrapping.segment();
    wrapping.update(0, false);
    wrapping.update(0.5, true);
    check(wrapping.segment() == segment+1, "undefined phase creates a plot gap and a fresh segment");

    const double resonantDistance = 5*std::cbrt(4*(sunMass+planets[1].mass)/(sunMass+planets[0].mass));
    OrbitDiagnostics matched, drifting;
    for (int i = 0; i <= 2400; ++i) {
        const double time = i*0.5;
        matched.record(fixture(time, resonantDistance), time);
        drifting.record(fixture(time, 8.4), time);
    }
    check(matched.history.size() == OrbitDiagnostics::capacity &&
          near(matched.history.back().time-matched.history.front().time, 1024), "measurement window is bounded to 1024 seconds");
    for (const auto& sample : matched.history) {
        check(sample.ratioValid && near(sample.periodRatio, 2), "period-ratio measurement recovers 2:1");
        check(sample.phaseValid[0] && sample.phaseValid[1], "eccentric fixture defines both critical angles");
        check(near(sample.phaseTurns[0], 0.5/(2*physicsPi)) && near(sample.phaseTurns[1], 0.4/(2*physicsPi)),
              "matched mean longitudes give constant 2:1 critical angles");
    }
    const double innerN = 2*physicsPi/initialPeriod(planets[0]);
    const double outerN = std::sqrt(gravity*(sunMass+planets[1].mass)/std::pow(8.4,3));
    for (int i = 0; i < 2; ++i) {
        const double turns = drifting.history.back().phaseTurns[i]-drifting.history.front().phaseTurns[i];
        check(near(turns, (2*outerN-innerN)*1024/(2*physicsPi), 1e-8) && std::abs(turns) > 3,
              "noncommensurate orbits produce measurable full phase turns");
    }

    Simulation simulation;
    Bodies independent = simulation.bodies;
    check(!simulation.diagnostics.history.front().phaseValid[0], "initial circular state does not claim a resonant angle");
    for (int i = 0; i < 512*256; ++i) {
        check(simulation.step(), "default simulation remains valid while collecting data");
        leapfrogStep(independent, physicsStep);
    }
    for (std::size_t i = 0; i < independent.size(); ++i)
        check(length(independent[i].position-simulation.bodies[i].position) < 1e-12 &&
              length(independent[i].velocity-simulation.bodies[i].velocity) < 1e-12,
              "diagnostics leave the gravitational trajectory unchanged");
    check(simulation.diagnostics.history.size() == 1025, "measurements use simulated time at half-second intervals");
    for (const auto& sample : simulation.diagnostics.history)
        check(sample.ratioValid && std::isfinite(sample.periodRatio) && sample.periodRatio > 1 &&
              std::isfinite(sample.elements[0].eccentricity), "evolving measurements remain finite and physically ordered");
    simulation.reset();
    check(simulation.diagnostics.history.size() == 1 && simulation.diagnostics.history.front().time == 0,
          "reset clears measurement history");

    std::cout << "Diagnostics tests passed: analytic elements, undefined periapsis, phase wrapping, fixed-phase\n"
                 "and drifting fixtures, bounded history, and unchanged three-body dynamics.\n";
}
