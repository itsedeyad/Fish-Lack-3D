#include "render/Textures.h"
#include "render/GLHeaders.h"
#include "core/MathUtil.h"
#include <cmath>
#include <initializer_list>
#include <vector>

namespace tex {

using namespace lf;

static GLuint g_ids[COUNT] = {};
static const int SIZE = 256;

// Tileable value noise: lattice coordinates wrap with the given period, so
// the texture repeats seamlessly when GL_REPEAT tiles it over a surface.
static float tileNoise(float x, float y, int period, uint32_t seed) {
    int xi = int(std::floor(x)), yi = int(std::floor(y));
    float fx = x - xi, fy = y - yi;
    auto h = [&](int a, int b) {
        a = ((a % period) + period) % period;
        b = ((b % period) + period) % period;
        return hash2i(a, b, seed);
    };
    float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    return lerpf(lerpf(h(xi, yi), h(xi + 1, yi), u), lerpf(h(xi, yi + 1), h(xi + 1, yi + 1), u), v);
}
static float tileFbm(float u, float v, int basePeriod, int octaves, uint32_t seed) {
    float sum = 0, amp = 0.5f, norm = 0;
    int p = basePeriod;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * tileNoise(u * p, v * p, p, seed + i * 31u);
        norm += amp; amp *= 0.5f; p *= 2;
    }
    return sum / norm;
}

static unsigned char to8(float v) { return (unsigned char)(clampf(v, 0.0f, 1.0f) * 255.0f + 0.5f); }

static void upload(Id id, const std::vector<unsigned char>& rgba) {
    glGenTextures(1, &g_ids[id]);
    glBindTexture(GL_TEXTURE_2D, g_ids[id]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // Each mip level averages a 2x2 block of the previous level.
    std::vector<unsigned char> pixels = rgba;
    for (int size = SIZE, level = 0; ; size /= 2, ++level) {
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        if (size == 1) break;
        int nextSize = size / 2;
        std::vector<unsigned char> next(nextSize * nextSize * 4);
        for (int y = 0; y < nextSize; ++y)
            for (int x = 0; x < nextSize; ++x)
                for (int c = 0; c < 4; ++c) {
                    int sum = 0;
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx)
                            sum += pixels[((y * 2 + dy) * size + x * 2 + dx) * 4 + c];
                    next[(y * nextSize + x) * 4 + c] = (sum + 2) / 4;
                }
        pixels = std::move(next);
    }
}

// Wood: long grain lines along U, warped by noise, plus knots and fine fibres.
static void makeWood(std::vector<unsigned char>& px) {
    for (int y = 0; y < SIZE; ++y)
        for (int x = 0; x < SIZE; ++x) {
            float u = float(x) / SIZE, v = float(y) / SIZE;
            float warp = tileFbm(u, v, 4, 4, 11) * 5.0f;
            float grain = 0.5f + 0.5f * std::sin((v * 18.0f + warp) * TWO_PI);
            grain = std::pow(grain, 3.0f);
            float fibre = tileNoise(u * 64, v * 4, 64, 5) * 0.5f + tileNoise(u * 128, v * 16, 128, 9) * 0.5f;
            float b = 0.80f - grain * 0.12f + (fibre - 0.5f) * 0.07f;
            Color c(0.98f * b, 0.80f * b, 0.60f * b);
            unsigned char* p = &px[(y * SIZE + x) * 4];
            p[0] = to8(c.r); p[1] = to8(c.g); p[2] = to8(c.b); p[3] = 255;
        }
}

// Water: soft caustic-like bright lines (|sin| of warped noise), near white so
// it modulates the per-vertex water colour computed by the water renderer.
static void makeWater(std::vector<unsigned char>& px) {
    for (int y = 0; y < SIZE; ++y)
        for (int x = 0; x < SIZE; ++x) {
            float u = float(x) / SIZE, v = float(y) / SIZE;
            float n1 = tileFbm(u, v, 4, 4, 21);
            float n2 = tileFbm(u + 0.37f, v + 0.11f, 8, 3, 33);
            float caustic = 1.0f - std::fabs(std::sin((n1 * 3.0f + n2) * PI * 2.0f));
            caustic = std::pow(caustic, 6.0f);
            float b = 0.91f + 0.04f * n2 + 0.12f * caustic;
            unsigned char* p = &px[(y * SIZE + x) * 4];
            p[0] = to8(b * 0.92f); p[1] = to8(b * 0.98f); p[2] = to8(b); p[3] = 255;
        }
}

// Ground detail: greyscale speckles and blotches; the terrain's vertex colours
// (sand, grass, dirt) are multiplied by it (GL_MODULATE).
static void makeGround(std::vector<unsigned char>& px) {
    for (int y = 0; y < SIZE; ++y)
        for (int x = 0; x < SIZE; ++x) {
            float u = float(x) / SIZE, v = float(y) / SIZE;
            float blotch = tileFbm(u, v, 4, 4, 41);
            float speck = tileNoise(u * 128, v * 128, 128, 43);
            float blades = tileNoise(u * 256, v * 32, 256, 47);
            float b = 0.80f + 0.16f * blotch + 0.14f * (speck - 0.5f) + 0.10f * (blades - 0.5f);
            unsigned char* p = &px[(y * SIZE + x) * 4];
            p[0] = p[1] = p[2] = to8(b);
            p[3] = 255;
        }
}

// Bark: vertical furrows (stretched noise along V).
static void makeBark(std::vector<unsigned char>& px) {
    for (int y = 0; y < SIZE; ++y)
        for (int x = 0; x < SIZE; ++x) {
            float u = float(x) / SIZE, v = float(y) / SIZE;
            float furrow = tileNoise(u * 16, v * 2, 16, 51);
            float detail = tileFbm(u, v, 8, 3, 53);
            float b = 0.55f + 0.35f * std::pow(furrow, 1.5f) + 0.15f * (detail - 0.5f);
            unsigned char* p = &px[(y * SIZE + x) * 4];
            p[0] = to8(b); p[1] = to8(b * 0.95f); p[2] = to8(b * 0.9f); p[3] = 255;
        }
}

void init() {
    std::vector<unsigned char> px(SIZE * SIZE * 4);
    makeWood(px);   upload(WOOD, px);
    makeWater(px);  upload(WATER, px);
    makeGround(px); upload(GROUND, px);
    makeBark(px);   upload(BARK, px);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void bind(Id id) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_ids[id]);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void unbind() {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}


} // namespace tex
