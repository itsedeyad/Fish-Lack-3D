#pragma once
#include "core/MathUtil.h"

namespace lf {
// Camera basis: right, up, and backward. OpenGL uses column-major matrices.
inline Mat4 viewMatrix(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 back = (eye - target).normalized();
    Vec3 right = cross(up, back).normalized();
    Vec3 vertical = cross(back, right);
    Mat4 m = Mat4::identity();
    m.m[0] = right.x; m.m[4] = right.y; m.m[8] = right.z;
    m.m[1] = vertical.x; m.m[5] = vertical.y; m.m[9] = vertical.z;
    m.m[2] = back.x; m.m[6] = back.y; m.m[10] = back.z;
    m.m[12] = -dot(right, eye);
    m.m[13] = -dot(vertical, eye);
    m.m[14] = -dot(back, eye);
    return m;
}

inline bool projectPoint(const Vec3& p, const double* view, const double* projection,
                         const int* viewport, float& sx, float& sy) {
    double world[4] = {p.x, p.y, p.z, 1}, camera[4] = {}, clip[4] = {};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) camera[r] += view[c * 4 + r] * world[c];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) clip[r] += projection[c * 4 + r] * camera[c];
    if (clip[3] <= 1e-8 || clip[2] < -clip[3] || clip[2] > clip[3]) return false;
    sx = float(viewport[0] + (clip[0] / clip[3] + 1) * viewport[2] * 0.5);
    // Centered viewport: include the letterbox offset in top-left screen coordinates.
    sy = float(viewport[1] + viewport[3] - (clip[1] / clip[3] + 1) * viewport[3] * 0.5);
    return true;
}
} // namespace lf
