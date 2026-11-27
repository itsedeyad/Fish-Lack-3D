// =============================================================================
//  Renderer.h - draws one frame of the 3D world.
//
//  Frame order (matters for depth testing and transparency):
//   1. sky dome (no depth writes)          5. angler, rod, line, bobber
//   2. terrain + scenery (opaque)          6. water surface (alpha blended)
//   3. pier, boat, props, soft shadows     7. lily pads, ripples, splashes,
//   4. fish (under water) + their shadows     aim helpers (blended, last)
// =============================================================================
#pragma once
#include "render/Atmosphere.h"
#include "render/Sky.h"
#include "render/Terrain.h"
#include "render/Pier.h"
#include "render/WaterRenderer.h"
#include "render/FishRenderer.h"
#include "render/RodRenderer.h"

namespace lf {
class Game;
class Camera;

class Renderer {
public:
    void init();
    void rebuildWorld();
    unsigned worldRebuilds() const {return rebuilds_;}
    void render(const Game& game, const Camera& cam, int w, int h);

    /// Matrices captured after the camera was applied (used by the HUD to
    /// project 3D points, e.g. the bite marker above the bobber).
    bool project(const Vec3& p, float& sx, float& sy) const;
    void screenDirection(const Vec3& p,float& dx,float& dy) const;
    const Atmosphere& atmosphere() const { return atm_; }

private:
    void setupLights(const Atmosphere& atm, bool topDown);
    void drawBucketCatch(const Game& game, float time) const;

    int loadedMap_=-1;unsigned rebuilds_=0;
    Atmosphere atm_;
    Sky sky_;
    Terrain terrain_;
    Pier pier_;
    WaterRenderer water_;
    FishRenderer fish_;
    RodRenderer rod_;
    double mv_[16] = {}, proj_[16] = {};
    int vp_[4] = {};
};

} // namespace lf
