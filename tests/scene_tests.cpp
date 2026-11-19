#include "scene.hpp"
#include "eclipse.hpp"
#include <cstdlib>
#include <iostream>

using namespace orbit;
namespace {
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(EXIT_FAILURE); }
}
bool near(double a, double b, double epsilon = 0.0001) { return std::abs(a - b) < epsilon; }
Vec3 transformPoint(const Mat4& m, Vec3 p) {
    return {m.data[0]*p.x + m.data[4]*p.y + m.data[8]*p.z + m.data[12],
            m.data[1]*p.x + m.data[5]*p.y + m.data[9]*p.z + m.data[13],
            m.data[2]*p.x + m.data[6]*p.y + m.data[10]*p.z + m.data[14]};
}
}
int main() {
    for (const auto& p : planets) {
        check(initialPeriod(p) > 0, "initial orbital period is positive");
    }
    check(initialPeriod(planets[1]) > initialPeriod(planets[0]), "outer planet initially moves more slowly");
    check(planets[0].orbitRadius > sunRadius + planets[0].radius, "inner planet clears the sun");
    check(planets[1].orbitRadius - planets[0].orbitRadius > planets[0].radius + planets[1].radius,
          "initial planet paths are well separated");

    const Bodies initial = initialBodies();
    const Vec3 orbitalAxis = angularMomentum(initial).asFloat();
    for (double spinRate : spinRates) {
        const Vec3 start = transformPoint(bodyTransform({}, 1, 0, spinRate), {1,0,0});
        const Vec3 next = transformPoint(bodyTransform({}, 1, 0.01, spinRate), {1,0,0});
        check(dot(cross(start, next-start), orbitalAxis) > 0, "each body's surface spin aligns with orbital angular momentum");
        const Vec3 quarterTurn = transformPoint(bodyTransform({3,4,5}, 2, physicsPi/(2*spinRate), spinRate), {1,0,0});
        check(length(quarterTurn-Vec3{3,4,7}) < 1e-5f, "prograde surface rotation moves +X toward +Z");
        for (double wrapTime : {2*physicsPi/spinRates[1], 2*physicsPi/spinRate}) {
            const Vec3 before = transformPoint(bodyTransform({}, 1, wrapTime-1e-4, spinRate), {1,0,0});
            const Vec3 after = transformPoint(bodyTransform({}, 1, wrapTime+1e-4, spinRate), {1,0,0});
            check(length(after-before) < 1e-4f, "surface spin remains continuous through planet and own angle wraps");
        }
    }
    for (std::size_t i = 1; i < initial.size(); ++i)
        check(dot(cross(initial[i].position-initial[0].position, initial[i].velocity-initial[0].velocity),
                  angularMomentum(initial)) > 0, "both initial planetary orbits are prograde");

    Simulation simulation;
    simulation.advance(2);
    check(near(simulation.time, 8), "default 1x advances four simulated seconds per real second");
    simulation.paused = true;
    simulation.advance(10);
    check(near(simulation.time, 8), "pause freezes simulation time");
    simulation.faster();
    simulation.paused = false;
    simulation.advance(3);
    check(near(simulation.time, 32), "speed changes simulated time");
    simulation.advance(-10);
    check(near(simulation.time, 32), "negative elapsed time is ignored");
    for (int i = 0; i < 100; ++i) simulation.faster();
    check(near(simulation.speed, 16), "maximum speed is bounded");
    for (int i = 0; i < 100; ++i) simulation.slower();
    check(near(simulation.speed, 1.0 / 64), "minimum speed is bounded");
    simulation.reset();
    check(near(simulation.time, 0) && near(simulation.speed, defaultSpeed) && !simulation.paused, "reset restores initial clock state");

    Camera camera;
    check(near(length(camera.eye()), camera.distance), "camera stays on its orbit");
    camera.rotate(100, 100000);
    check(camera.pitch < pi / 2, "camera avoids singularity at the pole");
    camera.zoom(100000);
    check(near(camera.distance, 2.5), "zoom cannot collapse into its target");
    camera.zoom(-100000);
    check(near(camera.distance, 48), "zoom out is bounded");
    const Mat4 view = lookAt(camera.eye(), {}, {0,1,0});
    check(length(transformPoint(view, camera.eye())) < 0.0001f, "view matrix puts the camera at the origin");
    const Vec3 target = transformPoint(view, {});
    check(near(target.x, 0) && near(target.y, 0) && near(target.z, -camera.distance), "camera looks toward the sun");
    const Vec3 modelPoint = transformPoint(translation({3,4,5}) * rotationY(pi / 2) * scale(2), {1,0,0});
    check(length(modelPoint - Vec3{3,4,3}) < 0.0001f, "model matrix applies scale, rotation, then translation");

    camera.target = {5,2,3};
    check(near(length(camera.eye() - camera.target), camera.distance), "focused camera orbits its target");
    check(length(transformPoint(lookAt(camera.eye(), camera.target, {0,1,0}), camera.target)
                 - Vec3{0,0,-camera.distance}) < 0.0001f, "focused view centres the planet");

    check(seekNextEclipse(simulation), "next eclipse is found by integrating forward");
    const double conjunction = simulation.time;
    check(conjunction > 0 && simulation.paused, "eclipse inspection pauses at a future state");
    const Vec3 sun = simulation.bodies[0].position.asFloat();
    const Vec3 inner = simulation.bodies[1].position.asFloat();
    const Vec3 outer = simulation.bodies[2].position.asFloat();
    check(dot(normalized(inner-sun), normalized(outer-sun)) > 0.99999f, "conjunction puts planets on the same sunward ray");
    const Vec3 noon = outer - normalized(outer-sun) * planets[1].radius;
    check(near(visibleSunFraction(noon, inner, planets[0].radius, sun), 0), "aligned inner planet totally eclipses the outer subsolar point");
    check(near(visibleSunFraction(inner, outer, planets[1].radius, sun), 1), "outer planet cannot shadow the inner planet from behind");
    Simulation integrated;
    while (integrated.time < conjunction) check(integrated.step(), "eclipse trajectory reaches the same time");
    for (std::size_t i = 0; i < 3; ++i)
        check(length(integrated.bodies[i].position - simulation.bodies[i].position) < 1e-12,
              "eclipse seeking preserves the physical trajectory");
    check(seekNextEclipse(simulation) && simulation.time > conjunction + 1, "next eclipse advances to a new event");
    check(near(visibleSunFraction({8,0,0}, {4,4,0}, 0.65f), 1), "off-axis planet casts no eclipse");
    check(near(visibleSunFraction({8,0,0}, {10,0,0}, 0.65f), 1), "planet behind the receiver casts no eclipse");
    check(near(visibleSunFraction({8,0,0}, {-4,0,0}, 0.65f), 1), "planet beyond the sun casts no eclipse");
    const float partial = visibleSunFraction({8,0,0}, {4,0.65f,0}, 0.65f);
    check(partial > 0 && partial < 1, "partial disk overlap gives a penumbra");
    check(visibleSunFraction({8,0,0}, {4,0.55f,0}, 0.65f) < partial &&
          visibleSunFraction({8,0,0}, {4,0.75f,0}, 0.65f) > partial, "penumbra fades as the blocker moves away");
    const float annular = visibleSunFraction({20,0,0}, {5,0,0}, 0.2f);
    check(annular > 0 && annular < 1, "a smaller apparent blocker cannot fully hide the sun");
    check(near(visibleSunFraction({8,0,0}, {4,0,0}, 0), 1), "zero-radius blocker leaves the sun visible");
    const Vec3 offset{10,3,-7};
    check(near(visibleSunFraction(Vec3{8,0,0}+offset, Vec3{4,0.65f,0}+offset, 0.65f, offset), partial),
          "eclipse follows a translated sun");

    std::cout << "All scene tests passed.\n";
}
