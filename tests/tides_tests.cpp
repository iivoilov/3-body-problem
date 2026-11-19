#include "scene.hpp"
#include "tides.hpp"
#include <cstdlib>
#include <iostream>

using namespace orbit;
namespace {
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(EXIT_FAILURE); }
}
TidalState binary(double eccentricity, double spin) {
    const double a = 5, m = 2, total = 1002, r = a*(1-eccentricity);
    const double v = std::sqrt(gravity*total/a*(1+eccentricity)/(1-eccentricity));
    return {{{{1000, {-r*m/total,0,0}, {0,0,-v*m/total}},
              {m, {r*1000/total,0,0}, {0,0,v*1000/total}}, {0, {1e6,0,0}, {}}}},
             {{{0,-spin,0}, {}}}, 0};
}
double circularizationError(double dt) {
    const TidalPlanets parameters{{{0.4,0.3,0.1,0.33}, {0,0,0,0.33}}};
    TidalState state = binary(0.08, 0.27);
    const double initial = tidalMechanicalEnergy(state, parameters);
    double error = 0;
    for (int step = 0; step < static_cast<int>(256/dt); ++step) {
        check(tidalStep(state, parameters, dt), "isolated binary stays separated");
        error = std::max(error, std::abs((tidalMechanicalEnergy(state, parameters)+state.heat-initial)/initial));
    }
    check(orbitalElements(state.bodies[0],state.bodies[1]).eccentricity < 0.08,
          "tides damp the isolated binary eccentricity");
    check(state.heat > 0, "orbital and spin dissipation generates heat");
    return error;
}
void compareSmallEccentricityTheory() {
    const TidalPlanets parameters{{{0.15,0.3,0.1,0.33}, {0,0,0,0.33}}};
    const double e = 0.05, e2 = e*e, n = std::sqrt(gravity*1002/125);
    // Hut's zero-obliquity, orbit-averaged pseudo-synchronous rotation.
    const double pseudo = n*(1+7.5*e2+5.625*e2*e2+0.3125*e2*e2*e2)
                       / ((1+3*e2+0.375*e2*e2)*std::pow(1-e2,1.5));
    auto state = binary(e,pseudo);
    double sumTime = 0, sumTime2 = 0, sumLogE = 0, sumTimeLogE = 0, samples = 0;
    constexpr double dt = 1.0/64;
    for (int step = 1; step <= 16384/dt; ++step) {
        check(tidalStep(state,parameters,dt), "theory comparison stays separated");
        if (step % 32 != 0) continue;
        const double t = step*dt;
        const double logE = std::log(orbitalElements(state.bodies[0],state.bodies[1]).eccentricity);
        ++samples; sumTime += t; sumTime2 += t*t; sumLogE += logE; sumTimeLogE += t*logE;
    }
    const double measured = -(samples*sumTimeLogE-sumTime*sumLogE)/(samples*sumTime2-sumTime*sumTime);
    const double predicted = 10.5*0.3*0.1*(1000.0/2)*std::pow(0.15/5,5)*n*n;
    check(std::abs(measured/predicted-1) < 0.1,
          "isolated low-e circularization agrees with the independent secular CTL prediction");
    std::cout << "Low-e damping rate: measured " << measured << ", secular prediction " << predicted << '\n';
}
}

int main() {
    const TidalPlanets parameters{{{0.4,0.3,0.1,0.33}, {0,0,0,0.33}}};
    auto state = binary(0.2, 0.1);
    const Vec3d initialMomentum = momentum(state.bodies), initialAngular = tidalAngularMomentum(state, parameters);
    const double initialEnergy = tidalMechanicalEnergy(state, parameters);
    tidalPairDissipation(state, parameters, 0, 100);
    check(state.heat > 0 && tidalMechanicalEnergy(state,parameters) < initialEnergy,
          "dissipative pair lowers mechanical energy");
    check(std::abs(tidalMechanicalEnergy(state,parameters)+state.heat-initialEnergy) < 1e-14,
          "exact pair step balances heat and mechanical energy");
    check(length(tidalAngularMomentum(state,parameters)-initialAngular) < 1e-14,
          "spin and orbital angular momentum exchange is balanced");
    check(length(momentum(state.bodies)-initialMomentum) < 1e-14,
          "primary back reaction conserves linear momentum");
    const double n = std::sqrt(gravity*1002/125);
    auto synchronous = binary(0,n);
    tidalPairDissipation(synchronous,parameters,0,100);
    check(synchronous.heat < 1e-30, "circular corotation has no dissipative force");
    auto fast = binary(0,2*n);
    const double orbitalBefore = -angularMomentum(fast.bodies).y;
    tidalPairDissipation(fast,parameters,0,100);
    check(-angularMomentum(fast.bodies).y > orbitalBefore && -fast.spins[0].y < 2*n,
          "fast prograde spin transfers angular momentum to the orbit");
    auto zeroLag = parameters;
    zeroLag[0].lag = 0;
    const auto unchanged = fast;
    tidalDissipation(fast,zeroLag,100);
    check(length(fast.bodies[1].velocity-unchanged.bodies[1].velocity) == 0 && fast.heat == unchanged.heat,
          "zero lag has no dissipation");
    const double coarse = circularizationError(1.0/16), fine = circularizationError(1.0/32);
    check(fine < coarse*0.3 && fine > coarse*0.2, "heat budget converges at second order");
    compareSmallEccentricityTheory();

    TidalState noTides{initialBodies(), {}, 0};
    auto reference = noTides.bodies;
    const TidalPlanets disabled{};
    for (int step = 0; step < 4096; ++step) {
        check(tidalStep(noTides,disabled,physicsStep), "gravity-only control stays separated");
        leapfrogStep(reference,physicsStep);
    }
    for (std::size_t i = 0; i < 3; ++i)
        check(length(reference[i].position-noTides.bodies[i].position) == 0 &&
              length(reference[i].velocity-noTides.bodies[i].velocity) == 0,
              "disabled tides reproduce existing leapfrog exactly");
    std::cout << "Tide tests passed; heat budget errors at h=1/16,1/32: " << coarse << ", " << fine << '\n';
}
