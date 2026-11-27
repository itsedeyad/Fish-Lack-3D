// =============================================================================
//  Fish.h - fish data and the FishSchool that moves every fish in the lake.
//
//  Movement model (simple steering, no physics engine):
//   * Every fish owns a predefined closed path: a tilted ellipse inside the lake.
//   * Each frame the fish looks at a point slightly ahead on its path and turns
//     towards it with a limited turn rate. This produces smooth curved swimming
//     and natural looking turns instead of snapping to the path.
//   * The same steering is reused for approaching the bobber, nibbling around
//     it and fleeing, so all transitions are smooth.
// =============================================================================
#pragma once
#include "core/MathUtil.h"
#include "game/Types.h"
#include <vector>

namespace lf {

enum class FishMode {
    Cruise,    // following its path
    Approach,  // swimming towards the bobber
    Nibble,    // circling and tasting the bait
    Bite,      // has taken the bait - hook window
    Hooked,    // fighting on the line (position driven by FishingSystem)
    Landed,    // being lifted out of the water
    Flee,      // escaping at speed
    Inactive,  // caught, waiting to respawn
};

struct Fish {
    bool toxic=false,suspicious=false;
    int   id = 0;
    int   type = FISH_BLUE;
    FishMode mode = FishMode::Cruise;
    Vec3  pos;
    float yaw = 0;          // heading (0 = +Z)
    float pitch = 0;        // nose up/down (used when hooked / landed)
    float roll = 0;
    float speed = 0;
    float turnRate = 0;     // current angular velocity, drives banking/tail
    float tailPhase = 0;
    float tailAmp = 0.3f;
    float modeTimer = 0;
    float cooldown = 0;     // seconds before it may be attracted again
    float respawnTimer = 0;
    float sizeScale = 1.0f; // per-individual size variation
    // predefined path (tilted ellipse)
    Vec3  pathCenter;
    float pathRx = 8, pathRz = 6, pathTilt = 0;
    float pathParam = 0, pathSpeed = 0.1f, pathDir = 1;
    float depth = 1.0f;
    float orbitAngle = 0;   // used while nibbling
    Vec3  fleeTarget;

    bool active() const { return mode != FishMode::Inactive; }
};

class FishSchool {
public:
    void init(Rng& rng);
    void update(float dt, float time, Rng& rng, int progression);

    /// Index of the closest fish that can be attracted to the bobber, or -1.
    int findCandidate(const Vec3& bobber, float radius, int preferredA=-1, int preferredB=-1) const;

    void startApproach(int i);
    void startNibble(int i);
    void startBite(int i);
    void setHooked(int i);
    void startLanding(int i);
    void flee(int i, const Vec3& from);
    void deactivate(int i);          // caught: remove and schedule a respawn

    /// Bobber position the engaged fish steers towards.
    Vec3 bobberTarget;

    std::vector<Fish> fish;

private:
    void respawn(Fish& f, Rng& rng, int progression);
    void makePath(Fish& f, Rng& rng);
    void steer(Fish& f, const Vec3& target, float targetSpeed, float dt);
    Vec3 pathPoint(const Fish& f, float param) const;
};

} // namespace lf
