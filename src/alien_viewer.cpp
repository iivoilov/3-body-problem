#include "render_utils.hpp" // GLEW before GLFW.
#include <GLFW/glfw3.h>
#include "alien_viewer.hpp"
#include "alien_world.hpp"
#include "hud.hpp"
#include <iostream>

namespace orbit {
namespace {
struct AlienApp {
    AlienSimulation simulation;
    Camera camera;
    bool surface = true, trails = true, showData = true, shadows = true;
    bool dragging = false, switchVersion = false;
    double lastX = 0, lastY = 0;
    float yaw = 2.93f, pitch = 0.035f;
    void reset() {
        simulation.reset(); camera = Camera{}; camera.distance = 19;
        yaw = 2.93f; pitch = 0.035f;
    }
};
AlienApp& alienApp(GLFWwindow* window) { return *static_cast<AlienApp*>(glfwGetWindowUserPointer(window)); }
void key(GLFWwindow* window, int code, int, int action, int) {
    if (action != GLFW_PRESS) return;
    auto& state = alienApp(window);
    switch (code) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        case GLFW_KEY_1: state.switchVersion = true; glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        case GLFW_KEY_V: state.surface = !state.surface; break;
        case GLFW_KEY_SPACE:
            if (!state.simulation.collision) state.simulation.paused = !state.simulation.paused;
            break;
        case GLFW_KEY_N: state.simulation.singleStep(); break;
        case GLFW_KEY_R: state.reset(); break;
        case GLFW_KEY_O: state.trails = !state.trails; break;
        case GLFW_KEY_D: state.showData = !state.showData; break;
        case GLFW_KEY_S: state.shadows = !state.shadows; break;
        case GLFW_KEY_LEFT_BRACKET: state.simulation.slower(); break;
        case GLFW_KEY_RIGHT_BRACKET: state.simulation.faster(); break;
        default: break;
    }
}
void mouse(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    auto& state = alienApp(window);
    glfwGetCursorPos(window,&state.lastX,&state.lastY);
    int width,height;
    glfwGetWindowSize(window,&width,&height);
    if (action == GLFW_PRESS && state.lastX > width-350 && state.lastY < 80) {
        if (state.lastY < 48) state.surface = !state.surface;
        else { state.switchVersion = true; glfwSetWindowShouldClose(window,GLFW_TRUE); }
    }
    state.dragging = action == GLFW_PRESS && state.lastY > 90 && state.lastY < height-(state.showData ? 178 : 0);
}
void cursor(GLFWwindow* window, double x, double y) {
    auto& state = alienApp(window);
    if (state.dragging) {
        if (state.surface) {
            state.yaw -= static_cast<float>(x-state.lastX)*0.004f;
            state.pitch = std::clamp(state.pitch+static_cast<float>(y-state.lastY)*0.003f,-0.8f,1.2f);
        } else state.camera.rotate(static_cast<float>(x-state.lastX),static_cast<float>(y-state.lastY));
    }
    state.lastX=x; state.lastY=y;
}
void smokePixels(const std::vector<unsigned char>& pixels, int width, int height, bool surface) {
    int warm = 0, lit = 0;
    // Only the scene; exclude text, header and footer.
    for (int y = 200; y < height-270; ++y)
        for (int x = 40; x < width-40; ++x) {
            const auto i = (static_cast<std::size_t>(y)*width+x)*3;
            if (pixels[i] > 150 && pixels[i+1] > 65 && pixels[i] > pixels[i+2]*1.2) ++warm;
            if (pixels[i]+pixels[i+1]+pixels[i+2] > 85) ++lit;
        }
    if (warm < (surface ? 100 : 30) || (surface && lit < 10000))
        throw std::runtime_error("Orbit / 02 scene has missing terrain or stars: warm="+std::to_string(warm)+", lit="+std::to_string(lit));
}
}

int runAlienViewer(const AlienViewerOptions& options) {
    glfwSetErrorCallback([](int code,const char* message) { std::cerr << "GLFW " << code << ": " << message << '\n'; });
    if (!glfwInit()) throw std::runtime_error("Cannot initialize GLFW. Run from a graphical desktop.");
    struct Lifetime { ~Lifetime() { glfwTerminate(); } } lifetime;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES,4);
    const bool hidden=options.smoke || !options.capture.empty();
    if (hidden) glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    GLFWwindow* window=glfwCreateWindow(1440,900,options.title.c_str(),nullptr,nullptr);
    if (!window) throw std::runtime_error("Cannot create Orbit / 02 graphics window.");
    glfwSetWindowSizeLimits(window,1000,720,GLFW_DONT_CARE,GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(hidden ? 0 : 1);
    glewExperimental=GL_TRUE;
    if (glewInit()!=GLEW_OK) throw std::runtime_error("Cannot initialize OpenGL extensions.");
    while (glGetError()!=GL_NO_ERROR) {}
    std::cout << "Orbit / 02 / GPU: " << glGetString(GL_RENDERER) << '\n';

    AlienApp state;
    state.reset(); state.surface=options.surface; state.simulation.speed=options.speed;
    while (state.simulation.time+alienStep<=options.startTime+1e-12)
        if (!state.simulation.step()) throw std::runtime_error("Surface contact during fast-forward.");
    glfwSetWindowUserPointer(window,&state);
    glfwSetKeyCallback(window,key);
    glfwSetMouseButtonCallback(window,mouse);
    glfwSetCursorPosCallback(window,cursor);
    glfwSetScrollCallback(window,[](GLFWwindow* w,double,double y) { if (!alienApp(w).surface) alienApp(w).camera.zoom(static_cast<float>(y)); });
    glfwSetWindowFocusCallback(window,[](GLFWwindow* w,int focused) { if (!focused) alienApp(w).dragging=false; });

    const Shader sky("../doc/shaders/alien-screen.vert","../doc/shaders/alien-sky.frag");
    const Shader body("body.vert","../doc/shaders/alien-body.frag");
    const Shader haloShader("../doc/shaders/alien-halo.vert","../doc/shaders/alien-halo.frag");
    const Shader simple("simple.vert","simple.frag");
    const Mesh quad({{{-1,-1,0},{}},{{1,-1,0},{}},{{1,1,0},{}},{{-1,1,0},{}}},{0,1,2,0,2,3},GL_TRIANGLES);
    const Mesh sphere=makeSphere();
    Mesh trailA({}, {}, GL_LINE_STRIP),trailB({}, {}, GL_LINE_STRIP),trailC({}, {}, GL_LINE_STRIP),trailPlanet({}, {}, GL_LINE_STRIP);
    const std::array<Mesh*,4> trailMeshes{{&trailA,&trailB,&trailC,&trailPlanet}};
    Hud hud;
    int framebufferWidth,framebufferHeight;
    glfwGetFramebufferSize(window,&framebufferWidth,&framebufferHeight);
    const OffscreenTarget offscreen(hidden,framebufferWidth,framebufferHeight);
    glEnable(GL_DEPTH_TEST); glEnable(GL_MULTISAMPLE);
    double previous=glfwGetTime();
    unsigned frame=0;
    std::vector<unsigned char> firstFrame;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const double now=glfwGetTime();
        if (!hidden) state.simulation.advance(std::min(now-previous,0.1));
        previous=now;
        if (options.smoke) {
            state.surface=frame==0 || frame==4;
            if (frame==1 || frame==3) for (int i=0;i<2048;++i) state.simulation.step();
            state.trails=frame!=3;
            if (frame==3) state.camera.rotate(70,25);
        }
        int width,height,logicalWidth,logicalHeight;
        glfwGetFramebufferSize(window,&width,&height);
        glfwGetWindowSize(window,&logicalWidth,&logicalHeight);
        if (!width || !height) { glfwWaitEventsTimeout(0.05); continue; }
        glViewport(0,0,width,height);
        glClearColor(0.004f,0.008f,0.014f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        std::array<Vec3,4> positions;
        float extent=8;
        for (std::size_t i=0;i<4;++i) {
            positions[i]=(i<3 ? state.simulation.stars[i].position : state.simulation.planet.position).asFloat();
            extent=std::max(extent,length(positions[i]));
        }
        const auto surfaceDirections=alienSkyDirections(state.simulation.stars,state.simulation.planet,state.simulation.time);
        const auto sample=state.simulation.measure();
        Vec3 forward,right,up,eye;
        if (state.surface) {
            forward={std::sin(state.yaw)*std::cos(state.pitch),std::sin(state.pitch),std::cos(state.yaw)*std::cos(state.pitch)};
            eye={0,3,0};
        } else {
            Camera fitted=state.camera;
            fitted.distance*=extent/8;
            eye=fitted.eye();
            forward=normalized(fitted.target-eye);
        }
        right=normalized(cross(forward,{0,1,0})); up=cross(right,forward);
        sky.use(); sky.viewport(static_cast<float>(width),static_cast<float>(height));
        sky.set("cameraForward",forward); sky.set("cameraRight",right); sky.set("cameraUp",up);
        sky.set("fovScale",state.surface ? 0.62f : std::tan(pi/8));
        sky.set("time",static_cast<float>(state.simulation.time));
        sky.set("surfaceView",state.surface ? 1 : 0); sky.set("shadowsEnabled",state.shadows ? 1 : 0);
        Vec3 angles;
        const auto angle=[&](int i) { return std::clamp(static_cast<float>(stellarRadii[i]*14/length(state.simulation.stars[i].position-state.simulation.planet.position)),0.009f,0.065f); };
        angles={angle(0),angle(1),angle(2)};
        sky.set("sunAngles",angles);
        sky.set("sunPowers",Vec3{static_cast<float>(sample.contributions[0]),static_cast<float>(sample.contributions[1]),static_cast<float>(sample.contributions[2])});
        for (int i=0;i<3;++i) {
            sky.set(("sunDirections["+std::to_string(i)+"]").c_str(),surfaceDirections[i]);
            sky.set(("sunColors["+std::to_string(i)+"]").c_str(),stellarColors[i]);
        }
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); quad.draw(); glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
        std::array<Vec3,4> bodyLabels{};
        if (!state.surface) {
            const Mat4 vp=perspective(pi/4,static_cast<float>(width)/height,0.01f,extent*8)*lookAt(eye,{}, {0,1,0});
            for (std::size_t i=0;i<4;++i) {
                const auto p=positions[i];
                const float clipW=vp.data[3]*p.x+vp.data[7]*p.y+vp.data[11]*p.z+vp.data[15];
                if (clipW<=0) continue;
                const float x=(vp.data[0]*p.x+vp.data[4]*p.y+vp.data[8]*p.z+vp.data[12])/clipW;
                const float y=(vp.data[1]*p.x+vp.data[5]*p.y+vp.data[9]*p.z+vp.data[13])/clipW;
                bodyLabels[i]={(x+1)*logicalWidth*0.5f,(1-y)*logicalHeight*0.5f,1};
            }
            simple.use(); simple.set("viewProjection",vp);
            if (state.trails) for (std::size_t i=0;i<4;++i) {
                std::vector<Vertex> vertices;
                for (auto p:state.simulation.trails[i]) vertices.push_back({p,{}});
                vertices.push_back({positions[i],{}});
                trailMeshes[i]->update(vertices);
                simple.set("color",i<3 ? stellarColors[i]*0.42f : Vec3{0.24f,0.65f,0.64f});
                trailMeshes[i]->draw();
            }
            body.use(); body.set("viewProjection",vp); body.set("eye",eye);
            body.set("time",static_cast<float>(state.simulation.time));
            for (int i=0;i<3;++i) {
                body.set(("starPositions["+std::to_string(i)+"]").c_str(),positions[i]);
                body.set(("starColors["+std::to_string(i)+"]").c_str(),stellarColors[i]);
            }
            for (int i=0;i<4;++i) {
                const float radius=i<3 ? stellarDisplayRadii[i] : 0.13f;
                body.set("model",bodyTransform(positions[i],radius,state.simulation.time,0.35));
                body.set("baseColor",i<3 ? stellarColors[i] : Vec3{0.26f,0.45f,0.48f});
                body.set("starBody",i<3 ? 1 : 0); sphere.draw();
            }
            haloShader.use(); haloShader.set("viewProjection",vp);
            haloShader.set("cameraRight",right); haloShader.set("cameraUp",up);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE); glDepthMask(GL_FALSE);
            for (int i=0;i<3;++i) {
                haloShader.set("center",positions[i]); haloShader.set("radius",stellarDisplayRadii[i]*6);
                haloShader.set("color",stellarColors[i]); quad.draw();
            }
            glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        }
        hud.drawAlien(logicalWidth,logicalHeight,state.simulation,state.surface,state.trails,state.showData,bodyLabels);
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL error in Orbit / 02.");
        const bool finalFrame=!options.smoke || frame==4;
        if (hidden) {
            const auto pixels=readFrame(width,height);
            if (options.smoke && (frame==0 || frame==1)) smokePixels(pixels,width,height,state.surface);
            if (options.smoke && frame==0) firstFrame=pixels;
            if (options.smoke && frame==4 && firstFrame==pixels) throw std::runtime_error("Orbit / 02 sky did not change with simulated time.");
            if (finalFrame && !options.capture.empty()) writeCapture(options.capture,width,height,pixels);
        }
        glfwSwapBuffers(window);
        if (!hidden && frame==0) {
            glFinish();
            std::cout << "First frame ready.\n" << std::flush;
        }
        ++frame;
        if (hidden && finalFrame) break;
    }
    if (options.smoke) std::cout << "Orbit / 02 graphics passed: terrain, three-star scene, views, camera, trails, evolving sky.\n";
    return state.switchVersion ? 1 : 0;
}
} // namespace orbit
