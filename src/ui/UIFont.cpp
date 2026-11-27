#include "ui/UIFont.h"
#include "render/GLHeaders.h"
#include <fstream>
#include <vector>
#include <cstdio>

namespace uifont {
namespace {
struct Glyph { float advance = 0, x = 0, y = 0, w = 0, h = 0, u = 0, v = 0, du = 0, dv = 0; };
struct Font { GLuint texture = 0; float height = 0; Glyph glyphs[256]; };
Font fonts[COUNT];
const float heights[COUNT] = {14, 20, 22, 32, 48, 80};
bool loaded = false;
float canvasScale = 1;
void* const bitmap[] = {GLUT_BITMAP_HELVETICA_12, GLUT_BITMAP_HELVETICA_18, GLUT_BITMAP_HELVETICA_18};
}

void init(const std::string& directory) {
    // These are already-rasterised artwork files, not runtime system fonts.
    // Loading needs only standard C++ streams and the fixed-function texture API.
    loaded = true;
    for (int i = 0; i < COUNT; ++i) {
        std::string base = directory + "/" + std::to_string(i);
        std::ifstream metrics(base + ".txt"), pixels(base + ".alpha", std::ios::binary);
        int width = 0, height = 0;
        metrics >> width >> height >> fonts[i].height;
        if (!metrics || !pixels || width <= 0 || height <= 0 || width > 4096 || height > 4096) { loaded = false; break; }
        for (Glyph& g : fonts[i].glyphs) {
            metrics >> g.advance >> g.x >> g.y >> g.w >> g.h >> g.u >> g.v >> g.du >> g.dv;
            g.u /= width; g.du /= width; g.v /= height; g.dv /= height;
        }
        std::vector<unsigned char> alpha(size_t(width) * height);
        pixels.read(reinterpret_cast<char*>(alpha.data()), std::streamsize(alpha.size()));
        if (!metrics || !pixels) { loaded = false; break; }
        glGenTextures(1, &fonts[i].texture);
        glBindTexture(GL_TEXTURE_2D, fonts[i].texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, width, height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, alpha.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    if (!loaded) {
        for (Font& font : fonts) { if (font.texture) glDeleteTextures(1, &font.texture); font.texture = 0; }
        std::printf("UI artwork missing or invalid; using built-in FreeGLUT fonts.\n");
    }
}

bool ready() { return loaded; }
void setCanvasScale(float scale) { canvasScale = scale; }
float width(int size, const std::string& text) {
    if (loaded) {
        float result = 0;
        for (unsigned char ch : text) result += fonts[size].glyphs[ch].advance;
        return result;
    }
    const auto* s = reinterpret_cast<const unsigned char*>(text.c_str());
    if (size <= MED && canvasScale == 1) return float(glutBitmapLength(bitmap[size], s));
    return glutStrokeLengthf(GLUT_STROKE_ROMAN, s) * heights[size] / 119.05f;
}

void draw(int size, float x, float y, const std::string& text) {
    if (loaded) {
        glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, fonts[size].texture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glBegin(GL_QUADS);
        for (unsigned char ch : text) {
            const Glyph& g = fonts[size].glyphs[ch];
            float left = x + g.x, top = y + g.y;
            glTexCoord2f(g.u, g.v); glVertex2f(left, top);
            glTexCoord2f(g.u + g.du, g.v); glVertex2f(left + g.w, top);
            glTexCoord2f(g.u + g.du, g.v + g.dv); glVertex2f(left + g.w, top + g.h);
            glTexCoord2f(g.u, g.v + g.dv); glVertex2f(left, top + g.h);
            x += g.advance;
        }
        glEnd();
        glPopAttrib();
        return;
    }
    const auto* s = reinterpret_cast<const unsigned char*>(text.c_str());
    if (size <= MED && canvasScale == 1) { glRasterPos2f(x, y); glutBitmapString(bitmap[size], s); return; }
    glPushMatrix();
    glTranslatef(x, y, 0);
    float k = heights[size] / 119.05f;
    glScalef(k, -k, 1);
    glutStrokeString(GLUT_STROKE_ROMAN, s);
    glPopMatrix();
}
} // namespace uifont
