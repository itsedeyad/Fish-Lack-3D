#include "render/Primitives.h"
#include "render/GLHeaders.h"
#include <cmath>
#include <initializer_list>

namespace prim {

using namespace lf;

void color(const Color& c) { glColor4f(c.r, c.g, c.b, c.a); }
void color(const Color& c, float alpha) { glColor4f(c.r, c.g, c.b, alpha); }

void specular(float s, float shininess) {
    GLfloat spec[4] = {s, s, s, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
}

// One face of a box: normal n, four corners, texture size in world units.
static void quad(const Vec3& n, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float tu, float tv) {
    glNormal3f(n.x, n.y, n.z);
    glTexCoord2f(0, 0);   glVertex3f(a.x, a.y, a.z);
    glTexCoord2f(tu, 0);  glVertex3f(b.x, b.y, b.z);
    glTexCoord2f(tu, tv); glVertex3f(c.x, c.y, c.z);
    glTexCoord2f(0, tv);  glVertex3f(d.x, d.y, d.z);
}

void box(float sx, float sy, float sz, float ts) {
    float x = sx * 0.5f, y = sy * 0.5f, z = sz * 0.5f;
    glBegin(GL_QUADS);
    quad({0, 0, 1},  {-x, -y, z},  {x, -y, z},  {x, y, z},   {-x, y, z},  sx * ts, sy * ts);
    quad({0, 0, -1}, {x, -y, -z},  {-x, -y, -z}, {-x, y, -z}, {x, y, -z},  sx * ts, sy * ts);
    quad({1, 0, 0},  {x, -y, z},   {x, -y, -z}, {x, y, -z},  {x, y, z},   sz * ts, sy * ts);
    quad({-1, 0, 0}, {-x, -y, -z}, {-x, -y, z}, {-x, y, z},  {-x, y, -z}, sz * ts, sy * ts);
    quad({0, 1, 0},  {-x, y, z},   {x, y, z},   {x, y, -z},  {-x, y, -z}, sx * ts, sz * ts);
    quad({0, -1, 0}, {-x, -y, -z}, {x, -y, -z}, {x, -y, z},  {-x, -y, z}, sx * ts, sz * ts);
    glEnd();
}

void boxFromTo(const Vec3& a, const Vec3& b, float ts) {
    glPushMatrix();
    glTranslatef((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f);
    box(std::fabs(b.x - a.x), std::fabs(b.y - a.y), std::fabs(b.z - a.z), ts);
    glPopMatrix();
}

void cylinder(float r0, float r1, float h, int slices, bool caps) {
    // Side normal of a cone is tilted by the slope (r0 - r1) / h.
    float slope = (r0 - r1) / (h > 1e-5f ? h : 1e-5f);
    float ny = slope / std::sqrt(1 + slope * slope);
    float nr = 1.0f / std::sqrt(1 + slope * slope);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= slices; ++i) {
        float a = TWO_PI * i / slices;
        float c = std::cos(a), s = std::sin(a);
        glNormal3f(c * nr, ny, s * nr);
        glTexCoord2f(float(i) / slices, h);
        glVertex3f(c * r1, h, s * r1);
        glTexCoord2f(float(i) / slices, 0);
        glVertex3f(c * r0, 0, s * r0);
    }
    glEnd();
    if (caps) {
        if (r1 > 0) {
            glBegin(GL_TRIANGLE_FAN);
            glNormal3f(0, 1, 0);
            glVertex3f(0, h, 0);
            for (int i = slices; i >= 0; --i) {
                float a = TWO_PI * i / slices;
                glVertex3f(std::cos(a) * r1, h, std::sin(a) * r1);
            }
            glEnd();
        }
        if (r0 > 0) {
            glBegin(GL_TRIANGLE_FAN);
            glNormal3f(0, -1, 0);
            glVertex3f(0, 0, 0);
            for (int i = 0; i <= slices; ++i) {
                float a = TWO_PI * i / slices;
                glVertex3f(std::cos(a) * r0, 0, std::sin(a) * r0);
            }
            glEnd();
        }
    }
}

void cylinderZ(float r0, float r1, float len, int slices, bool caps) {
    glPushMatrix();
    glRotatef(90, 1, 0, 0);   // Rx(90) maps the +Y axis onto +Z
    cylinder(r0, r1, len, slices, caps);
    glPopMatrix();
}

void cylinderBetween(const Vec3& a, const Vec3& b, float r0, float r1, int slices) {
    Vec3 d = b - a;
    float len = d.length();
    if (len < 1e-5f) return;
    Vec3 n = d / len;
    glPushMatrix();
    glTranslatef(a.x, a.y, a.z);
    // Rotate +Y onto the direction n: axis = Y x n, angle = acos(Y . n)
    Vec3 axis(n.z, 0, -n.x);
    float al = axis.length();
    if (al > 1e-5f) glRotatef(rad2deg(std::acos(clampf(n.y, -1, 1))), axis.x / al, 0, axis.z / al);
    else if (n.y < 0) glRotatef(180, 1, 0, 0);
    cylinder(r0, r1, len, slices, true);
    glPopMatrix();
}

void sphere(float r, int slices, int stacks) {
    for (int j = 0; j < stacks; ++j) {
        float p0 = PI * j / stacks - PI / 2, p1 = PI * (j + 1) / stacks - PI / 2;
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= slices; ++i) {
            float a = TWO_PI * i / slices;
            float c = std::cos(a), s = std::sin(a);
            Vec3 n1(std::cos(p1) * c, std::sin(p1), std::cos(p1) * s);
            Vec3 n0(std::cos(p0) * c, std::sin(p0), std::cos(p0) * s);
            glNormal3f(n1.x, n1.y, n1.z);
            glTexCoord2f(float(i) / slices, float(j + 1) / stacks);
            glVertex3f(n1.x * r, n1.y * r, n1.z * r);
            glNormal3f(n0.x, n0.y, n0.z);
            glTexCoord2f(float(i) / slices, float(j) / stacks);
            glVertex3f(n0.x * r, n0.y * r, n0.z * r);
        }
        glEnd();
    }
}

void hemisphere(float r, bool top, int slices, int stacks) {
    for (int j = 0; j < stacks; ++j) {
        float p0 = (PI / 2) * j / stacks, p1 = (PI / 2) * (j + 1) / stacks;
        if (!top) { p0 = -p0; p1 = -p1; }
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= slices; ++i) {
            float a = TWO_PI * i / slices * (top ? 1.0f : -1.0f);
            float c = std::cos(a), s = std::sin(a);
            Vec3 n1(std::cos(p1) * c, std::sin(p1), std::cos(p1) * s);
            Vec3 n0(std::cos(p0) * c, std::sin(p0), std::cos(p0) * s);
            glNormal3f(n1.x, n1.y, n1.z);
            glVertex3f(n1.x * r, n1.y * r, n1.z * r);
            glNormal3f(n0.x, n0.y, n0.z);
            glVertex3f(n0.x * r, n0.y * r, n0.z * r);
        }
        glEnd();
    }
}

void disc(float r, int slices) {
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0.5f, 0.5f);
    glVertex3f(0, 0, 0);
    for (int i = slices; i >= 0; --i) {
        float a = TWO_PI * i / slices;
        glTexCoord2f(0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a));
        glVertex3f(std::cos(a) * r, 0, std::sin(a) * r);
    }
    glEnd();
}

void torus(float innerR, float outerR, int sides, int rings) {
    float tube = (outerR - innerR) * 0.5f;
    float R = innerR + tube;
    for (int i = 0; i < rings; ++i) {
        float a0 = TWO_PI * i / rings, a1 = TWO_PI * (i + 1) / rings;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= sides; ++j) {
            float b = TWO_PI * j / sides;
            float cb = std::cos(b), sb = std::sin(b);
            for (float a : {a0, a1}) {
                float ca = std::cos(a), sa = std::sin(a);
                glNormal3f(ca * cb, sa * cb, sb);
                glVertex3f(ca * (R + tube * cb), sa * (R + tube * cb), tube * sb);
            }
        }
        glEnd();
    }
}

void blobShadow(float cx, float cy, float cz, float rx, float rz, float alpha) {
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0, 0, 0, alpha);
    glVertex3f(cx, cy, cz);
    glColor4f(0, 0, 0, 0);
    const int N = 16;
    for (int i = 0; i <= N; ++i) {
        float a = TWO_PI * i / N;
        glVertex3f(cx + std::cos(a) * rx, cy, cz + std::sin(a) * rz);
    }
    glEnd();
}

} // namespace prim
