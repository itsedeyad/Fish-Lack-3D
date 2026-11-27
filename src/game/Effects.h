// =============================================================================
//  Effects.h - water ripples and splash particles (simulation only).
//  The renderer reads these lists; update() advances them with elapsed time.
// =============================================================================
#pragma once
#include "core/MathUtil.h"
#include <vector>

namespace lf {

struct Ripple {
    Vec3  pos;
    float age = 0;
    float life = 2.0f;
    float maxRadius = 2.0f;
    float strength = 1.0f;   // 0..1, affects opacity and wave displacement
};

struct Particle {
    Vec3  pos, vel;
    float age = 0, life = 1.0f;
    float size = 0.06f;
    Color color;
};

class Effects {
    Rng particleRng_{471819u};
public:
    void clear() { ripples.clear(); particles.clear(); }
    void addRipple(const Vec3& p, float maxRadius, float life, float strength);
    /// Burst of water droplets flying up from p.
    void addSplash(const Vec3& p, int count, float power, Rng& rng);
    void update(float dt);

    std::vector<Ripple> ripples;
    std::vector<Particle> particles;
};

} // namespace lf
