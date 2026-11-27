#include "render/Sky.h"
#include "render/GLHeaders.h"
#include "render/Primitives.h"
#include <cmath>
#include <initializer_list>

namespace lf {

void Sky::init() {
    Rng rng(777);
    clouds_.clear();
    for (int i = 0; i < 16; ++i) {
        Cloud c;
        float a = rng.range(0.0f, TWO_PI);
        float d = rng.range(180.0f, 360.0f);
        c.pos = Vec3(std::cos(a) * d, rng.range(95.0f, 150.0f), std::sin(a) * d);
        int n = rng.irange(5, 9);
        float len = rng.range(25.0f, 50.0f);
        for (int k = 0; k < n; ++k) {
            Puff p;
            float t = (n == 1) ? 0.5f : float(k) / (n - 1);
            p.offset = Vec3((t - 0.5f) * len, rng.range(-2.0f, 4.0f), rng.range(-6.0f, 6.0f));
            p.r = rng.range(7.0f, 13.0f) * (1.0f - 0.5f * std::fabs(t - 0.5f));
            c.puffs.push_back(p);
        }
        clouds_.push_back(c);
    }
}

void Sky::draw(const Vec3& cam, const Atmosphere& atm, float time) const {
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);

    // ---- gradient dome: colour per vertex from elevation and sun proximity
    const float R = 450.0f;
    const int SL = 36, ST = 14;
    glPushMatrix();
    glTranslatef(cam.x, cam.y, cam.z);
    for (int j = 0; j < ST; ++j) {
        float e0 = -0.25f + (PI / 2 + 0.25f) * j / ST;
        float e1 = -0.25f + (PI / 2 + 0.25f) * (j + 1) / ST;
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= SL; ++i) {
            float a = TWO_PI * i / SL;
            for (float e : {e1, e0}) {
                Vec3 d(std::cos(e) * std::cos(a), std::sin(e), std::cos(e) * std::sin(a));
                float up = clampf(d.y, 0.0f, 1.0f);
                Color c = lerpc(atm.horizon, atm.zenith, std::pow(up, 0.55f));
                if (d.y < 0) c = lerpc(atm.horizon, scalec(atm.horizon, 0.75f), clampf(-d.y * 4, 0, 1));
                float s = std::fmax(0.0f, dot(d, atm.sunDir));
                float glow = 0.45f * std::pow(s, 6.0f) + 0.5f * std::pow(s, 48.0f);
                glow *= 0.6f + 0.4f * atm.progress;
                glColor3f(clampf(c.r + atm.sun.r * glow, 0, 1), clampf(c.g + atm.sun.g * glow, 0, 1),
                          clampf(c.b + atm.sun.b * glow, 0, 1));
                glVertex3f(d.x * R, d.y * R, d.z * R);
            }
        }
        glEnd();
    }

    // ---- sun disc: camera-facing billboard with a soft halo
    GLfloat mv[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    Vec3 right(mv[0], mv[4], mv[8]), up(mv[1], mv[5], mv[9]);
    Vec3 c = atm.sunDir * (R * 0.9f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    auto fan = [&](float radius, Color inner, float ia, Color outer, float oa) {
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(inner.r, inner.g, inner.b, ia);
        glVertex3f(c.x, c.y, c.z);
        glColor4f(outer.r, outer.g, outer.b, oa);
        for (int i = 0; i <= 32; ++i) {
            float a = TWO_PI * i / 32;
            Vec3 p = c + right * (std::cos(a) * radius) + up * (std::sin(a) * radius);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
    };
    fan(52.0f, atm.sun, 0.18f, atm.sun, 0.0f);           // halo
    Color core = lerpc(Color(1, 1, 0.95f), atm.sun, atm.progress * 0.6f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    fan(22.0f, core, 1.0f, atm.sun, 1.0f);                   // disc
    glPopMatrix();

    // ---- clouds: clusters of lit cuboids, drifting slowly
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LIGHTING);
    prim::specular(0.0f, 1.0f);
    float drift = std::fmod(time * 0.8f, 720.0f);
    for (const Cloud& cl : clouds_) {
        Vec3 p = cl.pos;
        p.x += drift;
        if (p.x > 360) p.x -= 720;
        // Keep cloud positions relative to the camera horizontally so they
        // always stay in the distant sky.
        glPushMatrix();
        glTranslatef(cam.x + p.x, p.y, cam.z + p.z);
        for (const Puff& pf : cl.puffs) {
            glPushMatrix();
            glTranslatef(pf.offset.x, pf.offset.y, pf.offset.z);
            glScalef(1.0f, 0.55f, 0.8f);
            glColor4f(atm.cloud.r, atm.cloud.g, atm.cloud.b, 0.92f);
            prim::box(pf.r*2.0f,pf.r*.70f,pf.r*1.5f);
            glPopMatrix();
        }
        glPopMatrix();
    }
    glPopAttrib();
}

} // namespace lf
