#pragma once

#include <algorithm>
#include <cmath>

// A small, explicit subset of graphics math. Matrices use OpenGL's column-major layout.
namespace orbit {
constexpr float pi = 3.14159265358979323846f;

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalized(Vec3 v) { return v * (1.0f / length(v)); }

struct Mat4 {
    float data[16]{};
    static Mat4 identity() {
        Mat4 m;
        m.data[0] = m.data[5] = m.data[10] = m.data[15] = 1;
        return m;
    }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 result;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                result.data[col * 4 + row] += a.data[k * 4 + row] * b.data[col * 4 + k];
    return result;
}

inline Mat4 translation(Vec3 v) {
    Mat4 m = Mat4::identity();
    m.data[12] = v.x; m.data[13] = v.y; m.data[14] = v.z;
    return m;
}
inline Mat4 scale(float s) {
    Mat4 m = Mat4::identity();
    m.data[0] = m.data[5] = m.data[10] = s;
    return m;
}
inline Mat4 rotationY(float angle) {
    Mat4 m = Mat4::identity();
    const float c = std::cos(angle), s = std::sin(angle);
    m.data[0] = c; m.data[2] = -s;
    m.data[8] = s; m.data[10] = c;
    return m;
}
inline Mat4 perspective(float fov, float aspect, float nearPlane, float farPlane) {
    Mat4 m;
    const float f = 1.0f / std::tan(fov / 2);
    m.data[0] = f / aspect;
    m.data[5] = f;
    m.data[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    m.data[11] = -1;
    m.data[14] = (2 * farPlane * nearPlane) / (nearPlane - farPlane);
    return m;
}
inline Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 forward = normalized(target - eye);
    const Vec3 right = normalized(cross(forward, up));
    const Vec3 cameraUp = cross(right, forward);
    Mat4 m = Mat4::identity();
    m.data[0] = right.x; m.data[4] = right.y; m.data[8] = right.z;
    m.data[1] = cameraUp.x; m.data[5] = cameraUp.y; m.data[9] = cameraUp.z;
    m.data[2] = -forward.x; m.data[6] = -forward.y; m.data[10] = -forward.z;
    m.data[12] = -dot(right, eye);
    m.data[13] = -dot(cameraUp, eye);
    m.data[14] = dot(forward, eye);
    return m;
}
} // namespace orbit
