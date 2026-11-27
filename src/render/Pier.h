// =============================================================================
//  Pier.h - the wooden fishing pier, its props (tackle box, bucket, lantern,
//  rope, life ring, crate) and the small rowing boat moored next to it.
// =============================================================================
#pragma once
#include "core/MathUtil.h"
#include "render/Atmosphere.h"

namespace lf {
class Game;

class Pier {
public:
    void init();
    /// Static pier + animated parts (boat on the waves, lantern, bucket catch).
    void draw(const Game& game, const Atmosphere& atm, float time) const;
    void drawShadows() const;
    static Vec3 lanternLightPos();

private:
    void buildPier();
    void buildBoat();
    unsigned pierList_ = 0, boatList_ = 0;
};

} // namespace lf
