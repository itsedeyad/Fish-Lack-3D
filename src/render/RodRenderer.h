// =============================================================================
//  RodRenderer.h - the angler, the fishing rod (hierarchical model), the
//  fishing line, the bobber, the aiming marker and the cast trajectory.
// =============================================================================
#pragma once
#include "core/MathUtil.h"
#include <vector>

namespace lf {
class Game;

class RodRenderer {
public:
    void drawAngler(const Game& g) const;
    void drawArms(const Game& g) const;
    void drawRod(const Game& g) const;
    void drawLine(const Game& g) const;
    void drawBobber(const Game& g, float time) const;
    /// Transparent helpers: aim ring on the water, predicted arc, cast trail.
    void drawAimHelpers(const Game& g, float time);

private:
    std::vector<Vec3> trail_;
};

} // namespace lf
