// =============================================================================
//  FishRenderer.h - procedural fish models.
//
//  Body: a "lofted" mesh - elliptical cross-sections placed along the body
//  axis with a radius profile (thin tail, thick middle, round nose). Vertex
//  colours paint the darker back, the light belly and per-species markings.
//  Hierarchy: body -> tail peduncle (rotates) -> caudal fin (rotates more),
//  plus dorsal/pectoral/pelvic fins and eyes.
// =============================================================================
#pragma once
#include "core/MathUtil.h"

namespace lf {
struct Fish;
class FishSchool;

class FishRenderer {
public:
    void init();
    void drawSchool(const FishSchool& school, float time) const;
    void drawShadows(const FishSchool& school) const;
    /// Draw one fish model at the origin facing +Z, length 1 * scale.
    void drawModel(int type, float length, float tailAngle, float finPhase) const;

private:
    unsigned bodyLists_ = 0;
};

} // namespace lf
