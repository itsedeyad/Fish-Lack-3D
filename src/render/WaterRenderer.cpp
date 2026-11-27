#include "render/WaterRenderer.h"
#include "core/Settings.h"
#include <algorithm>
#include "render/GLHeaders.h"
#include "render/Primitives.h"
#include "render/Textures.h"
#include "core/World.h"
#include "game/Effects.h"
#include <cmath>
#include <initializer_list>

namespace lf {

void WaterRenderer::init() {
    lilies_.clear();plants_.clear();
    step_ = settings::waterStep;
    x0_=-world::LAKE_RX-12;z0_=-world::LAKE_RZ-12;
    nx_=int(-2*x0_/step_)+1;nz_=int(-2*z0_/step_)+1;
    int n = nx_ * nz_;
    depth_.resize(n);
    rippleHeight_.resize(n);
    rippleX_.resize(n);
    rippleZ_.resize(n);
    pos_.resize(n * 3);
    nor_.resize(n * 3);
    col_.resize(n * 4);
    uv_.resize(n * 2);
    for (int j = 0; j < nz_; ++j)
        for (int i = 0; i < nx_; ++i) {
            float x = x0_ + i * step_, z = z0_ + j * step_;
            depth_[j * nx_ + i] = world::WATER_LEVEL - world::terrainHeight(x, z);
            uv_[(j * nx_ + i) * 2 + 0] = x * 0.15f;
            uv_[(j * nx_ + i) * 2 + 1] = z * 0.15f;
        }
    idx_.clear();
    for (int j = 0; j < nz_ - 1; ++j)
        for (int i = 0; i < nx_; ++i) {
            idx_.push_back((j + 1) * nx_ + i);
            idx_.push_back(j * nx_ + i);
        }

    // Lily pads in calm shallow water away from the pier.
    Rng rng(world::mapSpec(world::activeMap()).seed+8080);
    for (int k = 0; k < 4000 && lilies_.size() < (world::activeMap()==MapId::Marsh?65u:30u); ++k) {
        float x = rng.range(-world::LAKE_RX,world::LAKE_RX), z = rng.range(-world::LAKE_RZ,world::LAKE_RZ);
        float d = -world::terrainHeight(x, z);
        if (d < 0.3f || d > 1.8f || world::insidePier(x, z, 5.0f)) continue;
        if (distXZ(Vec3(x, 0, z), world::anglerPosition()) < 8.0f) continue;
        bool clash = false;
        for (const Lily& l : lilies_) if (distXZ(l.pos, Vec3(x, 0, z)) < 1.2f) clash = true;
        if (clash) continue;
        // pads come in small groups
        int group = rng.irange(1, 3);
        for (int g = 0; g < group; ++g) {
            Lily l;
            l.pos = Vec3(x + rng.range(-1.2f, 1.2f), 0, z + rng.range(-1.2f, 1.2f));
            l.r = rng.range(0.28f, 0.55f);
            l.rot = rng.range(0, 360);
            l.flower = rng.chance(0.3f);
            lilies_.push_back(l);
        }
    }
    // Decorative lake-bed plants, independent of fish AI and collision rules.
    for (int attempt = 0; attempt < 1500 && plants_.size() < 100; ++attempt) {
        float x = rng.range(-43, 43), z = rng.range(-34, 36);
        float bed = world::terrainHeight(x, z);
        if (bed < -0.6f && bed > -3.3f && !world::insidePier(x, z, 1))
            plants_.push_back(Vec3(x, bed + 0.03f, z));
    }

    const Vec3 beds[] = {Vec3(-7, 0, 18), Vec3(8, 0, 22), Vec3(-12, 0, 29)};
    for (const Vec3& bed : beds)
        for (int blade = 0; blade < 32; ++blade) {
            float x = bed.x + rng.range(-3, 3), z = bed.z + rng.range(-3, 3);
            float y = world::terrainHeight(x, z);
            if (y < -0.8f && !world::insidePier(x, z, 1)) plants_.push_back(Vec3(x, y + 0.03f, z));
        }

}

void WaterRenderer::updateRippleDisplacement(const Effects& fx) {
    // Visit only the grid rectangle touched by each travelling ripple ring.
    std::fill(rippleHeight_.begin(), rippleHeight_.end(), 0);
    std::fill(rippleX_.begin(), rippleX_.end(), 0);
    std::fill(rippleZ_.begin(), rippleZ_.end(), 0);
    for (const Ripple& r : fx.ripples) {
        float u = r.age / r.life;
        float front = r.maxRadius * u, radius = front + settings::rippleWidth;
        int i0 = std::max(0, int(std::floor((r.pos.x - radius - x0_) / step_)));
        int i1 = std::min(nx_ - 1, int(std::ceil((r.pos.x + radius - x0_) / step_)));
        int j0 = std::max(0, int(std::floor((r.pos.z - radius - z0_) / step_)));
        int j1 = std::min(nz_ - 1, int(std::ceil((r.pos.z + radius - z0_) / step_)));
        for (int j = j0; j <= j1; ++j)
            for (int i = i0; i <= i1; ++i) {
                float dx = x0_ + i * step_ - r.pos.x, dz = z0_ + j * step_ - r.pos.z;
                float dist = std::sqrt(dx * dx + dz * dz), offset = dist - front;
                if (std::fabs(offset) >= settings::rippleWidth) continue;
                float amp = 0.07f * r.strength * (1 - u);
                int k = j * nx_ + i;
                rippleHeight_[k] += amp * std::cos(offset * 3.5f) * (1 - std::fabs(offset) / settings::rippleWidth);
                if (dist > 1e-3f) {
                    rippleX_[k] += dx / dist * amp * 2;
                    rippleZ_[k] += dz / dist * amp * 2;
                }
            }
    }

}

void WaterRenderer::updateSurface(const Effects& fx, const Atmosphere& atm, const Vec3& cam, float time, bool topDown) {
    const auto& map=world::mapSpec(world::activeMap());const Color deep=map.deep,shallow=map.shallow;
    const Vec3 sun = atm.sunDir;
    float warmth=smoothstep(.55f,.94f,atm.progress);
    Color sunTint = lerpc(Color(1, 1, 1), atm.sun, 0.35f + .25f*warmth);

    updateRippleDisplacement(fx);

    for (int j = 0; j < nz_; ++j)
        for (int i = 0; i < nx_; ++i) {
            int k = j * nx_ + i;
            float x = x0_ + i * step_, z = z0_ + j * step_;
            float d = depth_[k];
            float h = world::waveHeight(x, z, time);
            Vec3 n = world::waveNormal(x, z, time);

            h += rippleHeight_[k];
            n.x += rippleX_[k]; n.z += rippleZ_[k];
            n = n.normalized();

            Vec3 p(x, h, z);
            Vec3 v = topDown ? Vec3(0, 1, 0) : (cam - p).normalized();
            float ndv = std::fmax(0.0f, dot(n, v));
            // A simple squared falloff keeps the sky reflection easy to explain.
            float grazing = 1.0f - ndv;
            float fres = 0.04f + 0.55f * grazing * grazing;

            Color base = lerpc(shallow, deep, smoothstep(0.0f, 4.2f, d));
            float ndl = std::fmax(0.0f, dot(n, sun));
            float light = 0.70f + 0.30f * ndl * atm.sunStrength;
            base = Color(base.r * light * sunTint.r, base.g * light * sunTint.g, base.b * light * sunTint.b);
            Vec3 refl = n * (2.0f * dot(n, v)) - v; // reflected view ray
            Color sky = lerpc(atm.horizon, atm.zenith, clampf(refl.y * 1.5f, 0.0f, 1.0f)*(1-.18f*warmth));
            Color c = lerpc(base, sky, clampf(fres * 0.65f, 0.0f, 0.75f));

            // Sun highlight: x^32 using five squarings, without expensive pow().
            Vec3 hv = (v + sun).normalized();
            float nh = std::fmax(0.0f, dot(n, hv));
            float highlight = nh * nh;
            for (int power = 0; power < 4; ++power) highlight *= highlight;
            float spec = highlight * (0.90f+.15f*warmth) * atm.sunStrength;
            c.r += atm.sun.r * spec; c.g += atm.sun.g * spec; c.b += atm.sun.b * spec;

            // Foam where the water meets the shore (and on wave crests near it).
            float foam = 1.0f - smoothstep(0.0f, 0.65f, d + (h - world::WATER_LEVEL) * 1.2f);
            foam *= 0.85f;
            c = lerpc(c, Color(0.95f, 0.97f, 0.97f), foam);

            // Stylised whitecaps: wave crests broken up by drifting noise.
            float crest = h + (valueNoise(x * 1.1f + time * 0.35f, z * 1.1f - time * 0.2f) - 0.5f) * 0.10f;
            float cap = smoothstep(0.105f, 0.14f, crest) * smoothstep(1.0f, 3.0f, d);
            c = lerpc(c, Color(0.95f, 0.98f, 1.0f), cap * 0.14f);

            float alpha = lerpf(0.26f, 0.72f, smoothstep(0.0f, 3.2f, d)) + fres * 0.30f + foam * 0.5f + cap * 0.3f;
            if (topDown) alpha -= 0.12f;

            pos_[k * 3 + 0] = x; pos_[k * 3 + 1] = h; pos_[k * 3 + 2] = z;
            nor_[k * 3 + 0] = n.x; nor_[k * 3 + 1] = n.y; nor_[k * 3 + 2] = n.z;
            col_[k * 4 + 0] = clampf(c.r, 0, 1);
            col_[k * 4 + 1] = clampf(c.g, 0, 1);
            col_[k * 4 + 2] = clampf(c.b, 0, 1);
            col_[k * 4 + 3] = clampf(alpha, 0, 0.95f);
        }

}

void WaterRenderer::draw(const Effects& fx, const Atmosphere& atm, const Vec3& cam, float time, bool topDown) {
    updateSurface(fx,atm,cam,time,topDown);

    glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_LIGHTING); // shading computed per vertex above
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    tex::bind(tex::WATER);
    // Scroll the texture over time with the texture matrix.
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(time * 0.012f, time * 0.007f, 0);
    glMatrixMode(GL_MODELVIEW);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, pos_.data());
    glNormalPointer(GL_FLOAT, 0, nor_.data());
    glColorPointer(4, GL_FLOAT, 0, col_.data());
    glTexCoordPointer(2, GL_FLOAT, 0, uv_.data());
    for (int j = 0; j < nz_ - 1; ++j)
        glDrawElements(GL_TRIANGLE_STRIP, nx_ * 2, GL_UNSIGNED_INT, &idx_[size_t(j) * nx_ * 2]);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    tex::unbind();
    glPopAttrib();
}

void WaterRenderer::drawPlants(float time) const {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    for (size_t i = 0; i < plants_.size(); ++i) {
        Vec3 root = plants_[i];
        float length = std::min(1.8f, -root.y * 0.75f);
        for (int blade = 0; blade < 4; ++blade) {
            float angle = blade * PI / 2 + float(i), width = 0.065f;
            Vec3 side(std::cos(angle) * width, 0, std::sin(angle) * width);
            glBegin(GL_TRIANGLE_STRIP);
            for (int segment = 0; segment <= 5; ++segment) {
                float u = segment / 5.0f;
                Vec3 p = root + Vec3(std::sin(time * 0.9f + i + u * 3) * u * 0.16f,
                                    u * length, std::cos(angle) * u * 0.22f);
                Vec3 half = side * (1 - u * 0.9f);
                glColor3f(0.10f + u * 0.07f, 0.38f + u * 0.18f, 0.31f + u * 0.16f);
                glVertex3f(p.x - half.x, p.y, p.z - half.z);
                glVertex3f(p.x + half.x, p.y, p.z + half.z);
            }
            glEnd();
        }
    }
    glPopAttrib();
}

void WaterRenderer::drawLilyPads(float time) const {
    prim::specular(0.3f, 30.0f);
    for (const Lily& l : lilies_) {
        float h = world::waveHeight(l.pos.x, l.pos.z, time);
        Vec3 n = world::waveNormal(l.pos.x, l.pos.z, time);
        glPushMatrix();
        glTranslatef(l.pos.x, h + 0.025f, l.pos.z);
        glRotatef(rad2deg(n.z), 1, 0, 0);
        glRotatef(-rad2deg(n.x), 0, 0, 1);
        glRotatef(l.rot + 5.0f * std::sin(time * 0.5f + l.pos.x), 0, 1, 0);
        // Pad: a disc with a wedge-shaped notch.
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(0, 1, 0);
        glColor3f(0.30f, 0.55f, 0.20f);
        glVertex3f(0, 0.01f, 0);
        glColor3f(0.18f, 0.42f, 0.14f);
        for (int i = 0; i <= 20; ++i) {
            float a = 0.35f + (TWO_PI - 0.45f) * i / 20.0f;
            glVertex3f(std::cos(a) * l.r, 0, std::sin(a) * l.r);
        }
        glEnd();
        if (l.flower) {
            glColor3f(1.0f, 0.75f, 0.85f);
            for (int p = 0; p < 6; ++p) {
                glPushMatrix();
                glRotatef(p * 60.0f, 0, 1, 0);
                glTranslatef(0.07f, 0.06f, 0);
                glRotatef(-35, 0, 0, 1);
                glScalef(1.0f, 0.35f, 0.5f);
                prim::sphere(0.08f, 6, 4);
                glPopMatrix();
            }
            glColor3f(1.0f, 0.85f, 0.2f);
            glTranslatef(0, 0.08f, 0);
            prim::sphere(0.04f, 6, 4);
        }
        glPopMatrix();
    }
}

void WaterRenderer::drawRipples(const Effects& fx, float time) const {
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (const Ripple& r : fx.ripples) {
        float u = r.age / r.life;
        for (float frac : {1.0f, 0.6f}) {
            float rad = r.maxRadius * u * frac;
            if (rad < 0.05f) continue;
            float a = r.strength * (1.0f - u) * (frac < 1 ? 0.45f : 0.7f);
            float w = 0.05f + 0.04f * (1 - u);
            glBegin(GL_QUAD_STRIP);
            for (int i = 0; i <= 32; ++i) {
                float ang = TWO_PI * i / 32;
                float c = std::cos(ang), s = std::sin(ang);
                float x0 = r.pos.x + c * (rad - w), z0 = r.pos.z + s * (rad - w);
                float x1 = r.pos.x + c * (rad + w), z1 = r.pos.z + s * (rad + w);
                glColor4f(0.92f, 0.97f, 1.0f, a);
                glVertex3f(x0, world::waveHeight(x0, z0, time) + 0.03f, z0);
                glColor4f(0.92f, 0.97f, 1.0f, a * 0.4f);
                glVertex3f(x1, world::waveHeight(x1, z1, time) + 0.03f, z1);
            }
            glEnd();
        }
    }
    glPopAttrib();
}

void WaterRenderer::drawParticles(const Effects& fx) const {
    if (fx.particles.empty()) return;
    GLfloat mv[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    Vec3 right(mv[0], mv[4], mv[8]), up(mv[1], mv[5], mv[9]);
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);
    for (const Particle& p : fx.particles) {
        float a = p.color.a * (1.0f - p.age / p.life);
        glColor4f(p.color.r, p.color.g, p.color.b, a);
        Vec3 r = right * p.size, u = up * p.size;
        Vec3 c = p.pos;
        Vec3 v0 = c - r - u, v1 = c + r - u, v2 = c + r + u, v3 = c - r + u;
        glVertex3f(v0.x, v0.y, v0.z);
        glVertex3f(v1.x, v1.y, v1.z);
        glVertex3f(v2.x, v2.y, v2.z);
        glVertex3f(v3.x, v3.y, v3.z);
    }
    glEnd();
    glPopAttrib();
}

} // namespace lf
