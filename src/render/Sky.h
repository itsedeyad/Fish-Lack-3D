// =============================================================================
//  Sky.h - gradient sky dome, sun disc with glow, and drifting clouds.
// =============================================================================
#pragma once
#include "render/Atmosphere.h"
#include <vector>

namespace lf {

class Sky {
public:
    void init();
    /// Draw centred on the camera (the sky is "infinitely" far away).
    void draw(const Vec3& camPos, const Atmosphere& atm, float time) const;

private:
    struct Puff { Vec3 offset; float r; };
    struct Cloud { Vec3 pos; std::vector<Puff> puffs; };
    std::vector<Cloud> clouds_;
};

} // namespace lf
