// =============================================================================
//  WaterRenderer.h - the animated lake surface.
//
//  A regular grid mesh is rebuilt every frame:
//    height  = sum of travelling sine waves + ring-shaped ripple displacement
//    normal  = analytic derivative of the waves
//    colour  = depth-based teal gradient, simple sky reflection, sun glitter
//              and shoreline foam, computed per vertex (simple CPU shading)
//    alpha   = depth based, so the sandy shallows and the fish show through
//  A procedural texture scrolled with the texture matrix adds moving detail.
// =============================================================================
#pragma once
#include "core/MathUtil.h"
#include "render/Atmosphere.h"
#include <vector>

namespace lf {
class Effects;
class Game;

class WaterRenderer {
public:
    void init();
    void draw(const Effects& fx, const Atmosphere& atm, const Vec3& camPos, float time, bool topDown);
    void drawPlants(float time) const;
    void drawLilyPads(float time) const;
    void drawRipples(const Effects& fx, float time) const;
    void drawParticles(const Effects& fx) const;

private:
    void updateRippleDisplacement(const Effects& fx);
    void updateSurface(const Effects& fx,const Atmosphere& atm,const Vec3& cam,float time,bool topDown);
    int nx_ = 0, nz_ = 0;
    float x0_ = 0, z0_ = 0, step_ = 1.0f;
    std::vector<float> rippleHeight_, rippleX_, rippleZ_; // local ripple displacement
    std::vector<float> depth_;               // static: water depth per vertex
    std::vector<float> pos_, nor_, col_, uv_; // per-frame vertex arrays
    std::vector<unsigned> idx_;               // static triangle-strip indices
    struct Lily { Vec3 pos; float r, rot; bool flower; };
    std::vector<Lily> lilies_;
    std::vector<Vec3> plants_;
};

} // namespace lf
