#pragma once

#include "gpu.hpp"
#include "scene.hpp"
#include <fstream>
#include <iostream>
#include <random>

namespace orbit {
inline Mesh makeSphere() {
    constexpr unsigned rings = 40, segments = 64;
    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;
    for (unsigned y = 0; y <= rings; ++y) {
        const float latitude = pi * y / rings;
        for (unsigned x = 0; x <= segments; ++x) {
            const float longitude = 2 * pi * x / segments;
            const Vec3 p{std::sin(latitude) * std::cos(longitude), std::cos(latitude),
                         std::sin(latitude) * std::sin(longitude)};
            vertices.push_back({p, p}); // On a unit sphere, the position is also the surface normal.
        }
    }
    for (unsigned y = 0; y < rings; ++y)
        for (unsigned x = 0; x < segments; ++x) {
            const unsigned a = y * (segments + 1) + x, b = a + segments + 1;
            indices.insert(indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    return Mesh(vertices, indices, GL_TRIANGLES);
}
inline Mesh makeStars() {
    std::mt19937 random(42); // Reproducible sky for screenshots and experiments.
    std::uniform_real_distribution<float> unit(-1, 1);
    std::vector<Vertex> vertices;
    for (int i = 0; i < 1600; ++i) {
        const float y = unit(random), angle = (unit(random) + 1) * pi;
        const float radius = std::sqrt(1 - y * y);
        vertices.push_back({Vec3{radius * std::cos(angle), y, radius * std::sin(angle)} * 90, {}});
    }
    return Mesh(vertices, {}, GL_POINTS);
}

// Hidden windows do not have a reliable default framebuffer on every window system.
// Give smoke tests and captures their own color/depth storage instead.
class OffscreenTarget {
public:
    OffscreenTarget(bool enabled, int width, int height) {
        if (!enabled) return;
        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glGenRenderbuffers(1, &color_);
        glBindRenderbuffer(GL_RENDERBUFFER, color_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_);
        glGenRenderbuffers(1, &depth_);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            release();
            throw std::runtime_error("Cannot create the offscreen framebuffer.");
        }
    }
    ~OffscreenTarget() { release(); }
    OffscreenTarget(const OffscreenTarget&) = delete;
    OffscreenTarget& operator=(const OffscreenTarget&) = delete;
private:
    GLuint framebuffer_ = 0, color_ = 0, depth_ = 0;
    void release() {
        glDeleteRenderbuffers(1, &depth_);
        glDeleteRenderbuffers(1, &color_);
        glDeleteFramebuffers(1, &framebuffer_);
    }
};

inline std::vector<unsigned char> readFrame(int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Cannot read rendered pixels.");
    return pixels;
}

inline void writeCapture(const std::string& path, int width, int height, const std::vector<unsigned char>& pixels) {
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot write capture: " + path);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int row = height - 1; row >= 0; --row)
        file.write(reinterpret_cast<const char*>(pixels.data() + static_cast<std::size_t>(row) * width * 3), width * 3);
    if (!file) throw std::runtime_error("Failed writing capture: " + path);
    std::cout << "Captured " << path << '\n';
}

} // namespace orbit
