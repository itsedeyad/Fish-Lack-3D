// =============================================================================
//  MathUtil.h - small math toolkit used by both game logic and rendering.
//
//  Vec3  : 3D vector (positions, directions, colours)
//  Mat4  : 4x4 matrix stored column-major exactly like OpenGL expects, so a
//          Mat4 can be passed straight to glMultMatrixf(). The fishing rod uses
//          it to compute its hierarchical transform chain on the CPU (to know
//          where the rod tip is) and then draws with the very same matrices.
//  Rng   : tiny deterministic random generator (xorshift32)
//  noise : value-noise + fractal Brownian motion for procedural content
// =============================================================================
#pragma once
#include <cmath>
#include <cstdint>

namespace lf {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

inline float deg2rad(float d) { return d * PI / 180.0f; }
inline float rad2deg(float r) { return r * 180.0f / PI; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
/// Wrap an angle into [-PI, PI].
inline float wrapAngle(float a) {
    while (a > PI) a -= TWO_PI;
    while (a < -PI) a += TWO_PI;
    return a;
}
/// Frame-rate independent exponential smoothing factor.
/// Moving a value towards a target with lerp(v, target, damp(rate, dt))
/// gives the same motion regardless of the frame rate.
inline float damp(float rate, float dt) { return 1.0f - std::exp(-rate * dt); }

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    constexpr Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float lengthXZ() const { return std::sqrt(x * x + z * z); }
    Vec3 normalized() const { float l = length(); return l > 1e-6f ? *this / l : Vec3(0, 1, 0); }
};
inline Vec3 operator*(float s, const Vec3& v) { return v * s; }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 lerpv(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
inline float distXZ(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}
/// Unit vector on the XZ plane for a yaw angle. Yaw 0 points along +Z and the
/// same convention is used by glRotatef(yawDeg, 0,1,0) applied to a +Z model.
inline Vec3 yawDir(float yaw) { return {std::sin(yaw), 0.0f, std::cos(yaw)}; }

// ----------------------------------------------------------------- Mat4 ----
struct Mat4 {
    float m[16]; // column-major: element (row r, col c) is m[c*4 + r]

    static Mat4 identity() {
        Mat4 r{};
        for (int i = 0; i < 16; ++i) r.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        return r;
    }
    static Mat4 translate(float x, float y, float z) {
        Mat4 r = identity();
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }
    static Mat4 translate(const Vec3& v) { return translate(v.x, v.y, v.z); }
    static Mat4 scale(float x, float y, float z) {
        Mat4 r = identity();
        r.m[0] = x; r.m[5] = y; r.m[10] = z;
        return r;
    }
    static Mat4 rotateX(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
        return r;
    }
    static Mat4 rotateY(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
        return r;
    }
    static Mat4 rotateZ(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
        return r;
    }
    Mat4 operator*(const Mat4& b) const {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 4; ++rr) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + rr] * b.m[c * 4 + k];
                r.m[c * 4 + rr] = s;
            }
        return r;
    }
    Vec3 transformPoint(const Vec3& p) const {
        return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
    }
    Vec3 transformDir(const Vec3& d) const {
        return {m[0] * d.x + m[4] * d.y + m[8] * d.z,
                m[1] * d.x + m[5] * d.y + m[9] * d.z,
                m[2] * d.x + m[6] * d.y + m[10] * d.z};
    }
};

// ------------------------------------------------------------------ Rng ----
class Rng {
public:
    explicit Rng(uint32_t seed = 12345u) : s_(seed ? seed : 1u) {}
    uint32_t next() {
        s_ ^= s_ << 13; s_ ^= s_ >> 17; s_ ^= s_ << 5;
        return s_;
    }
    float uniform() { return (next() & 0xFFFFFF) / float(0x1000000); } // [0,1)
    float range(float a, float b) { return a + (b - a) * uniform(); }
    int irange(int a, int b) { return a + int(next() % uint32_t(b - a + 1)); } // [a,b]
    bool chance(float p) { return uniform() < p; }
    void seed(uint32_t s) { s_ = s ? s : 1u; }
private:
    uint32_t s_;
};

// ---------------------------------------------------------------- noise ----
inline float hash2i(int x, int z, uint32_t seed = 0) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(z) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFF) / float(0xFFFFFF);
}
/// Smoothly interpolated value noise in [0,1].
inline float valueNoise(float x, float z, uint32_t seed = 0) {
    int xi = int(std::floor(x)), zi = int(std::floor(z));
    float fx = x - xi, fz = z - zi;
    float u = fx * fx * (3 - 2 * fx), v = fz * fz * (3 - 2 * fz);
    float a = hash2i(xi, zi, seed), b = hash2i(xi + 1, zi, seed);
    float c = hash2i(xi, zi + 1, seed), d = hash2i(xi + 1, zi + 1, seed);
    return lerpf(lerpf(a, b, u), lerpf(c, d, u), v);
}
/// Fractal Brownian motion: several octaves of value noise, result in ~[0,1].
inline float fbm(float x, float z, int octaves = 4, uint32_t seed = 0) {
    float sum = 0, amp = 0.5f, norm = 0, f = 1;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise(x * f, z * f, seed + i * 17u);
        norm += amp; amp *= 0.5f; f *= 2.0f;
    }
    return sum / norm;
}

// ---------------------------------------------------------------- Color ----
struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    Color() = default;
    constexpr Color(float R, float G, float B, float A = 1.0f) : r(R), g(G), b(B), a(A) {}
};
inline Color lerpc(const Color& a, const Color& b, float t) {
    return {lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t), lerpf(a.a, b.a, t)};
}
inline Color scalec(const Color& c, float s) { return {c.r * s, c.g * s, c.b * s, c.a}; }

} // namespace lf
