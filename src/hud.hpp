#pragma once

#include "gpu.hpp"
#include "scene.hpp"
#include <string>
#include <vector>

namespace orbit {
inline constexpr int panelWidth = 270;
inline constexpr int diagnosticsHeight = 240;

// Tiny built-in bitmap lettering: no font files, textures, or UI dependencies.
class Hud {
public:
    Hud();
    ~Hud();
    Hud(const Hud&) = delete;
    Hud& operator=(const Hud&) = delete;
    void draw(int width, int height, const Simulation& simulation, bool wireframe, bool orbits,
              bool shadows, float eclipseCoverage, bool showDiagnostics);
private:
    struct PixelVertex { float x, y, r, g, b; };
    Shader shader_{"hud.vert", "hud.frag"};
    GLuint vao_ = 0, vbo_ = 0;
    std::vector<PixelVertex> vertices_;
    void rect(float x, float y, float w, float h, Vec3 color);
    void text(float x, float y, const std::string& value, float size, Vec3 color);
    void line(float x0, float y0, float x1, float y1, float thickness, Vec3 color);
    void diagnostics(int width, int height, const OrbitDiagnostics& data);
    void plot(float x, float y, float width, float height, const OrbitDiagnostics& data,
              int kind, double low, double high);
};
} // namespace orbit
