#include "gpu.hpp" // GLEW must be included before GLFW.
#include "render_utils.hpp"
#include "alien_viewer.hpp"
#include <GLFW/glfw3.h>
#include "hud.hpp"
#include "scene.hpp"
#include "eclipse.hpp"
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

using namespace orbit;

namespace {
struct App {
    Simulation simulation;
    Camera camera;
    bool wireframe = false, orbits = true, dragging = false;
    bool shadows = true, eclipseFocus = false;
    bool showDiagnostics = true;
    bool switchVersion = false;
    double lastX = 0, lastY = 0;
    void reset() {
        simulation.reset();
        camera = Camera{};
        wireframe = false;
        orbits = true;
        dragging = false;
        shadows = true;
        eclipseFocus = false;
        showDiagnostics = true;
    }
    bool inspectEclipse() {
        if (!seekNextEclipse(simulation)) {
            std::cerr << "No upcoming eclipse found within four initial outer periods; state unchanged.\n";
            return false;
        }
        const Vec3 position = simulation.bodies[2].position.asFloat();
        const Vec3 fromSun = position - simulation.bodies[0].position.asFloat();
        camera.target = position;
        camera.yaw = std::atan2(-fromSun.x, -fromSun.z) + 0.35f;
        camera.pitch = 0.5f;
        camera.distance = 5;
        eclipseFocus = true;
        shadows = true;
        wireframe = false;
        return true;
    }
};

App& app(GLFWwindow* window) { return *static_cast<App*>(glfwGetWindowUserPointer(window)); }
void onKey(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    auto& state = app(window);
    switch (key) {
        case GLFW_KEY_2: state.switchVersion = true; glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        case GLFW_KEY_SPACE:
            if (!state.simulation.collision) state.simulation.paused = !state.simulation.paused;
            break;
        case GLFW_KEY_N: state.simulation.singleStep(); break;
        case GLFW_KEY_LEFT_BRACKET: state.simulation.slower(); break;
        case GLFW_KEY_RIGHT_BRACKET: state.simulation.faster(); break;
        case GLFW_KEY_R: state.reset(); break;
        case GLFW_KEY_W: state.wireframe = !state.wireframe; break;
        case GLFW_KEY_O: state.orbits = !state.orbits; break;
        case GLFW_KEY_E: state.inspectEclipse(); break;
        case GLFW_KEY_S: state.shadows = !state.shadows; break;
        case GLFW_KEY_D: state.showDiagnostics = !state.showDiagnostics; break;
        default: break;
    }
}
void onMouseButton(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    auto& state = app(window);
    glfwGetCursorPos(window, &state.lastX, &state.lastY);
    int width = 0, height = 0;
    glfwGetWindowSize(window, &width, &height);
    state.dragging = action == GLFW_PRESS && state.lastX >= panelWidth &&
                     (!state.showDiagnostics || state.lastY < height-diagnosticsHeight);
}
void onCursor(GLFWwindow* window, double x, double y) {
    auto& state = app(window);
    if (state.dragging)
        state.camera.rotate(static_cast<float>(x - state.lastX), static_cast<float>(y - state.lastY));
    state.lastX = x;
    state.lastY = y;
}
void onScroll(GLFWwindow* window, double, double y) { app(window).camera.zoom(static_cast<float>(y)); }

void drawBody(const Mesh& sphere, const Shader& shader, Vec3 position, float radius,
              double time, Vec3 color, int kind) {
    shader.set("model", bodyTransform(position, radius, time, spinRates[kind]));
    shader.set("baseColor", color);
    shader.set("bodyKind", kind);
    sphere.draw();
}

void checkFrame(const std::vector<unsigned char>& pixels, int width, int height, int sidebar, int dataHeight) {
    int warm = 0, blue = 0;
    // Exclude the overlay and sidebar. Count pixels from the sun and inner planet.
    const int sceneHeight = height-dataHeight;
    for (int y = dataHeight+sceneHeight/8; y < dataHeight+sceneHeight*7/8; ++y)
        for (int x = sidebar; x < width; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * width + x) * 3;
            const int r = pixels[i], g = pixels[i + 1], b = pixels[i + 2];
            if (r > 140 && r > b * 2 && g > 40) ++warm;
            if (b > 70 && b > r * 1.4 && g > 40) ++blue;
        }
    if (warm < 100 || blue < 30)
        throw std::runtime_error("Graphics smoke test failed: sun or blue planet missing (warm pixels: "
                                 + std::to_string(warm) + ", blue pixels: " + std::to_string(blue) + ").");
}

void checkEclipse(const std::vector<unsigned char>& shadowed, const std::vector<unsigned char>& unshadowed,
                  int width, int height, int sidebar, int dataHeight) {
    int darkened = 0;
    const int sceneHeight = height-dataHeight;
    for (int y = dataHeight+sceneHeight/8; y < dataHeight+sceneHeight*7/8; ++y)
        for (int x = sidebar; x < width; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * width + x) * 3;
            const int before = unshadowed[i] + unshadowed[i+1] + unshadowed[i+2];
            const int after = shadowed[i] + shadowed[i+1] + shadowed[i+2];
            if (before - after > 30) ++darkened;
        }
    if (darkened < 500)
        throw std::runtime_error("Eclipse graphics test failed: shadow did not darken the planet.");
    std::cout << "Eclipse comparison passed: " << darkened << " surface pixels darkened.\n";
}

struct Options {
    bool smoke = false, eclipse = false, resonance = false, orbit02 = false, surface = true;
    double startTime = 0, speed = defaultSpeed;
    std::string capture, title = "3-body-problem | Orbit / 01";
};
Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--orbit" && i+1 < argc) {
            const std::string version = argv[++i];
            if (version != "01" && version != "02") throw std::runtime_error("--orbit must be 01 or 02.");
            options.orbit02 = version == "02";
        }
        else if (argument == "--view" && i+1 < argc) {
            const std::string view = argv[++i];
            if (view != "surface" && view != "orbit") throw std::runtime_error("--view must be surface or orbit.");
            options.surface = view == "surface";
        }
        else if (argument == "--smoke-test") options.smoke = true;
        else if (argument == "--eclipse") options.eclipse = true;
        else if (argument == "--resonance") options.resonance = true;
        else if (argument == "--capture" && i + 1 < argc) options.capture = argv[++i];
        else if (argument == "--title" && i + 1 < argc) options.title = argv[++i];
        else if ((argument == "--time" || argument == "--speed") && i + 1 < argc) {
            const std::string value = argv[++i];
            std::size_t consumed = 0;
            const double number = std::stod(value, &consumed);
            if (consumed != value.size() || !std::isfinite(number) ||
                (argument == "--time" ? number < 0 : number < 1.0/64 || number > 16))
                throw std::runtime_error("Invalid value for " + argument + ": " + value);
            if (argument == "--time") options.startTime = number;
            else options.speed = number;
        }
        else if (argument == "--help") {
            std::cout << "Usage: three_body_problem [--orbit 01|02] [--view surface|orbit] [--resonance] [--time value] [--speed factor] [--eclipse]\n"
                         "                    [--smoke-test] [--capture image.ppm]\n"
                         "No options: open Orbit / 01 (one star and two planets).\n"
                         "--orbit 02: open the alien world; V switches between surface and orbital views.\n"
                         "1 / 2: switch between Orbit / 01 and Orbit / 02 in the native app.\n"
                         "--resonance: use the eccentric, phase-aligned 2:1 candidate (same initial 2.05 period ratio).\n"
                         "--eclipse: pause at the next alignment and focus on Ember's shadow.\n"
                         "--time: integrate forward (seconds in Orbit / 01; years in Orbit / 02).\n"
                         "--speed: factor from 1/64 to 16; 1x = 4 seconds/s in 01 or 0.08 years/s in 02.\n"
                         "--smoke-test: check normal rendering and compare eclipse shadows on/off.\n"
                         "--capture: render a hidden frame to a PPM image and exit.\n"
                         "--title: set the window title (useful for recording one specific window).\n"
                         "Hidden rendering still requires a working graphics display.\n";
            std::exit(EXIT_SUCCESS);
        } else throw std::runtime_error("Unknown or incomplete option: " + argument);
    }
    return options;
}

int run(const Options& options) {
    glfwSetErrorCallback([](int code, const char* message) {
        std::cerr << "GLFW error " << code << ": " << message << '\n';
    });
    if (!glfwInit()) throw std::runtime_error("Cannot initialize GLFW. Run from a graphical desktop session.");
    struct GlfwLifetime { ~GlfwLifetime() { glfwTerminate(); } } glfwLifetime;

    // A context is the driver's collection of OpenGL state for this window.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    const bool hidden = options.smoke || !options.capture.empty();
    if (hidden) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1280, 800, options.title.c_str(), nullptr, nullptr);
    if (!window) throw std::runtime_error("Cannot create an OpenGL 3.3 Core window.");
    glfwSetWindowSizeLimits(window, 900, 720, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(hidden ? 0 : 1);
    glewExperimental = GL_TRUE;
    const GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK)
        throw std::runtime_error(reinterpret_cast<const char*>(glewGetErrorString(glewStatus)));
    // Some GLEW/driver combinations leave GL_INVALID_ENUM while probing a core context.
    while (glGetError() != GL_NO_ERROR) {}
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\nGPU: " << glGetString(GL_RENDERER) << '\n';

    App state;
    if (options.resonance) state.simulation = Simulation{true};
    if (!options.smoke) {
        while (state.simulation.time+physicsStep <= options.startTime+1e-12)
            if (!state.simulation.step()) throw std::runtime_error("Surface contact reached during fast-forward.");
    }
    state.simulation.speed = options.speed;
    if (options.eclipse && !options.smoke && !state.inspectEclipse())
        throw std::runtime_error("Cannot find an upcoming eclipse.");
    glfwSetWindowUserPointer(window, &state);
    glfwSetKeyCallback(window, onKey);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursor);
    glfwSetScrollCallback(window, onScroll);
    glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) {
        if (!focused) app(w).dragging = false;
    });

    // Build GPU resources once. Each frame reuses the same sphere for all three bodies.
    const Shader body("body.vert", "body.frag"), simple("simple.vert", "simple.frag"), glow("glow.vert", "glow.frag");
    const Mesh sphere = makeSphere();
    Mesh sunTrail({}, {}, GL_LINE_STRIP), innerTrail({}, {}, GL_LINE_STRIP), outerTrail({}, {}, GL_LINE_STRIP);
    const std::array<Mesh*, 3> trailMeshes{{&sunTrail, &innerTrail, &outerTrail}};
    const Mesh stars = makeStars();
    const Mesh halo({{{-1,-1,0},{}}, {{1,-1,0},{}}, {{1,1,0},{}}, {{-1,1,0},{}}}, {0,1,2,0,2,3}, GL_TRIANGLES);
    Hud hud;
    int targetWidth = 0, targetHeight = 0;
    glfwGetFramebufferSize(window, &targetWidth, &targetHeight);
    const OffscreenTarget offscreen(hidden, targetWidth, targetHeight);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_PROGRAM_POINT_SIZE);

    double lastTime = glfwGetTime();
    unsigned frame = 0;
    std::vector<unsigned char> eclipseFrame;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const double now = glfwGetTime();
        // Cap large gaps (debugging, moving the window, waking from sleep).
        if (!hidden) state.simulation.advance(std::min(now - lastTime, 0.1));
        lastTime = now;
        if (options.smoke && frame < 3) {
            if (frame > 0)
                for (int i = 0; i < static_cast<int>(3 / physicsStep); ++i) state.simulation.step();
            state.wireframe = frame == 1;
            state.orbits = frame != 2;
            state.showDiagnostics = frame != 1;
            if (frame == 1) state.camera.rotate(100, 40);
            if (frame == 2) state.camera.zoom(2);
        } else if (options.smoke && frame == 3) {
            state.reset();
            if (!state.inspectEclipse()) throw std::runtime_error("Eclipse search failed during graphics test.");
        } else if (options.smoke) {
            state.shadows = false;
        }

        const std::array<Vec3, 3> positions{{state.simulation.bodies[0].position.asFloat(),
                                           state.simulation.bodies[1].position.asFloat(),
                                           state.simulation.bodies[2].position.asFloat()}};
        if (state.eclipseFocus) state.camera.target = positions[2];

        int width, height, logicalWidth, logicalHeight;
        glfwGetFramebufferSize(window, &width, &height);
        glfwGetWindowSize(window, &logicalWidth, &logicalHeight);
        if (width == 0 || height == 0) { glfwWaitEventsTimeout(0.05); continue; }
        const int sidebar = static_cast<int>(panelWidth * static_cast<double>(width) / logicalWidth);
        const int sceneWidth = width - sidebar;
        const int dataHeight = state.showDiagnostics ? static_cast<int>(diagnosticsHeight * static_cast<double>(height) / logicalHeight) : 0;
        const int sceneHeight = height-dataHeight;
        glViewport(sidebar, dataHeight, sceneWidth, sceneHeight);
        glClearColor(0.014f, 0.023f, 0.043f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const Vec3 eye = state.camera.eye();
        const Mat4 viewProjection = perspective(pi / 4, static_cast<float>(sceneWidth) / sceneHeight, 0.1f, 200)
                                  * lookAt(eye, state.camera.target, {0,1,0});

        // 1. Distant stars and the actual trajectories recorded by the physics solver.
        simple.use();
        simple.set("viewProjection", viewProjection);
        simple.set("pointSize", 1.6f * static_cast<float>(width) / logicalWidth);
        simple.set("color", Vec3{0.40f, 0.49f, 0.62f});
        stars.draw();
        if (state.orbits) {
            for (std::size_t i = 0; i < trailMeshes.size(); ++i) {
                std::vector<Vertex> vertices;
                vertices.reserve(state.simulation.trails[i].size() + 1);
                for (Vec3 point : state.simulation.trails[i]) vertices.push_back({point, {}});
                vertices.push_back({positions[i], {}});
                trailMeshes[i]->update(vertices);
                simple.set("color", i == 0 ? Vec3{0.5f, 0.35f, 0.12f} : planets[i-1].color * 0.4f);
                trailMeshes[i]->draw();
            }
        }

        // 2. Solid geometry. Depth testing makes nearer surfaces hide farther ones.
        body.use();
        body.set("viewProjection", viewProjection);
        body.set("eye", eye);
        body.set("time", static_cast<float>(std::fmod(state.simulation.time, 1000.0)));
        body.set("sunRadius", sunRadius);
        body.set("sunPosition", positions[0]);
        body.set("shadowsEnabled", state.shadows ? 1 : 0);
        if (state.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        drawBody(sphere, body, positions[0], sunRadius, state.simulation.time, {1,0.7f,0.2f}, 0);
        for (std::size_t i = 0; i < planets.size(); ++i) {
            const Planet& p = planets[i];
            const std::size_t other = 1 - i;
            body.set("occluderCenter", positions[other+1]);
            body.set("occluderRadius", planets[other].radius);
            drawBody(sphere, body, positions[i+1], p.radius, state.simulation.time, p.color, static_cast<int>(i) + 1);
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // 3. A camera-facing, softly blended quad gives the sun a simple halo.
        if (!state.wireframe) {
            const Vec3 forward = normalized(state.camera.target - eye);
            const Vec3 right = normalized(cross(forward, {0,1,0}));
            glow.use();
            glow.set("viewProjection", viewProjection);
            glow.set("cameraRight", right);
            glow.set("cameraUp", cross(right, forward));
            glow.set("sunPosition", positions[0]);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDepthMask(GL_FALSE);
            halo.draw();
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // 4. Overlay in logical window coordinates, independent of the 3D camera.
        glViewport(0, 0, width, height);
        const Vec3 dayCenter = positions[2] - normalized(positions[2] - positions[0]) * planets[1].radius;
        const float coverage = 1 - visibleSunFraction(dayCenter, positions[1], planets[0].radius, positions[0]);
        hud.draw(logicalWidth, logicalHeight, state.simulation, state.wireframe, state.orbits,
                 state.shadows, coverage, state.showDiagnostics);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR)
            throw std::runtime_error("OpenGL error after rendering: " + std::to_string(error));
        const bool finalFrame = !options.smoke || frame == 4;
        if (hidden) {
            const auto pixels = readFrame(width, height);
            if (finalFrame && !options.capture.empty()) writeCapture(options.capture, width, height, pixels);
            if (options.smoke && !state.eclipseFocus) checkFrame(pixels, width, height, sidebar, dataHeight);
            if (options.smoke && frame == 3) eclipseFrame = pixels;
            if (options.smoke && frame == 4) checkEclipse(eclipseFrame, pixels, width, height, sidebar, dataHeight);
        }
        glfwSwapBuffers(window);
        if (!hidden && frame == 0) {
            glFinish();
            std::cout << "First frame ready.\n" << std::flush;
        }
        ++frame;
        if (hidden && finalFrame) break;
    }
    if (options.smoke) std::cout << "Graphics smoke test passed: shading, wireframe, camera, orbit toggle, eclipse shadows.\n";
    return state.switchVersion ? 2 : EXIT_SUCCESS;
}
} // namespace

int main(int argc, char** argv) {
    try {
        auto options = parseOptions(argc, argv);
        if (options.orbit02 && (options.resonance || options.eclipse))
            throw std::runtime_error("--resonance and --eclipse belong to Orbit / 01.");
        for (;;) {
            const int result = options.orbit02
                ? runAlienViewer({options.smoke, options.surface, options.startTime, options.speed, options.capture,
                                  options.title == "3-body-problem | Orbit / 01" ? "3-body-problem | Orbit / 02" : options.title})
                : run(options);
            if (result == 0) return EXIT_SUCCESS;
            options.orbit02 = result == 2;
            options.startTime = 0;
            options.eclipse = false;
        }
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
