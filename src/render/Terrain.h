// =============================================================================
//  Terrain.h - ground mesh and all static scenery around the lake: trees,
//  rocks, bushes, reeds, flowers and distant mountains.
//
//  Static geometry is compiled once into OpenGL display lists, so the GPU
//  driver stores the vertex data and each frame needs only glCallList().
// =============================================================================
#pragma once
#include "core/MathUtil.h"
#include <vector>

namespace lf {

class Terrain {
public:
    void init();
    void drawGround() const;            // textured height field
    void drawScenery(float time, float lampGlow) const; // trees, rocks, plants, mountains
    void drawShadows() const;           // soft blob shadows (blended)

private:
    void buildGround();
    void buildScenery();
    void buildShadows();
    void buildRockShapes();

    struct Placed { Vec3 pos; float scale; float rot; int kind; Color tint; };
    std::vector<Placed> trees_, rocks_, bushes_;
    unsigned groundList_ = 0, sceneryList_ = 0, shadowList_ = 0, rockShapes_ = 0;
    static constexpr int ROCK_SHAPES = 6;
};

} // namespace lf
