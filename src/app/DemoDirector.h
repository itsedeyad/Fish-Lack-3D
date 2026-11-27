// =============================================================================
//  DemoDirector.h - automated demonstration / runtime test mode.
//
//  Started with:  LakeFishing3D --demo            (bot plays, endless)
//                 LakeFishing3D --shots <folder>  (bot plays a scripted
//                     session, saves screenshots of every game phase and a
//                     log of all state transitions, then exits)
//  The bot presses the same keys a player would (see game/AutoPlayer), so
//  this exercises the real game loop, renderer and HUD end-to-end.
// =============================================================================
#pragma once
#include "game/AutoPlayer.h"
#include <cstdio>
#include <set>
#include <string>

namespace lf {
class Camera;

class DemoDirector {
public:
    DemoDirector(const std::string& shotDir, bool takeShots);
    ~DemoDirector();
    void update(float dt, Game& g, InputState& in, Camera& cam);
    /// Name of a screenshot to save after the next rendered frame ("" = none).
    std::string takeShotRequest();
    bool finished() const { return finished_; }
    const std::string& shotDir() const { return dir_; }
    void log(const char* fmt, ...);

private:
    void shot(const std::string& name);
    bool taken(const std::string& name) const { return taken_.count(name) > 0; }

    AutoPlayer bot_{2026};
    std::string dir_;
    bool shots_ = false, finished_ = false;
    std::set<std::string> taken_;
    std::string request_;
    FILE* log_ = nullptr;
    int phase_ = 0;
    float clock_ = 0, phaseTimer_ = 0, pendingTimer_ = -1;
    std::string pendingShot_;
    int pendingAction_ = 0;  // 1 = restore camera, 2 = unpause, 3 = press ENTER
    int postAction_ = 0;
    GameState lastState_ = GameState::MainMenu;
};

} // namespace lf
