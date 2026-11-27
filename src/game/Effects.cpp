#include "game/Effects.h"
#include <algorithm>

namespace lf {

void Effects::addRipple(const Vec3& p, float maxRadius, float life, float strength) {
    if (ripples.size() > 40) ripples.erase(ripples.begin());
    Ripple r;
    r.pos = p;
    r.maxRadius = maxRadius;
    r.life = life;
    r.strength = strength;
    ripples.push_back(r);
}

void Effects::addSplash(const Vec3& p, int count, float power, Rng&) {
    // Visual particle count must never change fishing probabilities.
    Rng& rng=particleRng_;
    for (int i = 0; i < count; ++i) {
        Particle q;
        float a = rng.range(0.0f, TWO_PI);
        float out = rng.range(0.3f, 1.0f) * power;
        q.pos = p + Vec3(std::cos(a) * 0.1f, 0.05f, std::sin(a) * 0.1f);
        q.vel = Vec3(std::cos(a) * out, rng.range(1.5f, 3.5f) * power, std::sin(a) * out);
        q.life = rng.range(0.5f, 1.0f);
        q.size = rng.range(0.06f, 0.14f);
        float w = rng.range(0.75f, 1.0f);
        q.color = Color(w, w + 0.0f, 1.0f, 0.9f);
        particles.push_back(q);
    }
    if (particles.size() > 600) particles.erase(particles.begin(), particles.begin() + (particles.size() - 600));
}

void Effects::update(float dt) {
    for (Ripple& r : ripples) r.age += dt;
    ripples.erase(std::remove_if(ripples.begin(), ripples.end(),
                                 [](const Ripple& r) { return r.age >= r.life; }),
                  ripples.end());

    const float gravity = -9.0f;
    for (Particle& p : particles) {
        p.age += dt;
        p.vel.y += gravity * dt;
        p.pos += p.vel * dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
                                   [](const Particle& p) { return p.age >= p.life || p.pos.y < -0.3f; }),
                    particles.end());
}

} // namespace lf
