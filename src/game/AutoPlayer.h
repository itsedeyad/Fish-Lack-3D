// =============================================================================
//  AutoPlayer.h - a simple bot that plays the game by pressing the same keys a
//  human would (it writes into InputState). Used for:
//    * automated headless gameplay tests (tests/test_gameplay.cpp)
//    * the --demo mode that records screenshots of real gameplay
//  Behaviour flags let tests provoke specific outcomes (line break, miss, ...).
// =============================================================================
#pragma once
#include "game/Game.h"

namespace lf {

class AutoPlayer {
public:
    struct Options {
        bool reelAlways = false;   // never release R  -> line breaks
        bool neverHook = false;    // ignore bites     -> "too late"
        bool hookEarly = false;    // press E on nibble -> "too early"
        bool neverReel = false;    // never press R    -> slack / run escape
        bool startFromMenu = true;
        float reaction = 0.30f;    // seconds before reacting to a bite
    };
    explicit AutoPlayer(uint32_t seed = 99u) : rng_(seed) {}
    Options opt;

    /// Decide which keys to press for this frame.
    void drive(const Game& g, InputState& in, float dt);

private:
    void press(InputState& in, unsigned char k);
    void hold(InputState& in, unsigned char k, bool on);
    Rng rng_;
    bool  hasTarget_ = false;
    float targetYaw_ = 0, targetPitch_ = 0.75f, targetPower_ = 0.7f;
    float reactTimer_ = 0;
    float menuCooldown_ = 0;
};

} // namespace lf
