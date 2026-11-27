#include "app/DemoDirector.h"
#include "render/Camera.h"
#include <cstdarg>

namespace lf {

DemoDirector::DemoDirector(const std::string& shotDir, bool takeShots) : dir_(shotDir), shots_(takeShots) {
    if (shots_) {
        std::string path = dir_ + "/demo_log.txt";
        log_ = std::fopen(path.c_str(), "w");
    }
    log("Demo director started (%s)", shots_ ? "scripted screenshot session" : "endless demo");
}

DemoDirector::~DemoDirector() {
    if (log_) std::fclose(log_);
}

void DemoDirector::log(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::printf("[%7.2fs] %s\n", clock_, buf);
    std::fflush(stdout);
    if (log_) { std::fprintf(log_, "[%7.2fs] %s\n", clock_, buf); std::fflush(log_); }
}

void DemoDirector::shot(const std::string& name) {
    if (!shots_ || taken(name)) return;
    taken_.insert(name);
    request_ = name;
    log("screenshot %s", name.c_str());
}

std::string DemoDirector::takeShotRequest() {
    std::string r = request_;
    request_.clear();
    return r;
}

void DemoDirector::update(float dt, Game& g, InputState& in, Camera& cam) {
    clock_ += dt;
    phaseTimer_ += dt;

    if (g.state != lastState_) {
        log("state %-16s -> %-16s caught=%d/%d time_left=%.0f tension=%.2f",
            stateName(lastState_), stateName(g.state), g.caught, g.target, g.timeLeft, g.fishing.tension());
        lastState_ = g.state;
    }

    // Actions that must happen only AFTER the screenshot frame was rendered.
    if (postAction_ != 0 && request_.empty()) {
        if (postAction_ == 1) cam.setMode(CameraMode::Gameplay);
        if (postAction_ == 2) { in.keyDown('p'); in.keyUp('p'); }
        if (postAction_ == 3) { in.keyDown(KEY_ENTER); in.keyUp(KEY_ENTER); }
        postAction_ = 0;
        return;
    }
    // A screenshot that needed a set-up (camera switch, pause) is pending.
    if (pendingTimer_ >= 0) {
        pendingTimer_ -= dt;
        if (pendingTimer_ < 0) {
            shot(pendingShot_);
            postAction_ = pendingAction_;
            pendingAction_ = 0;
        }
        return; // freeze the bot while setting up the shot
    }

    if (!shots_) {
        // Endless demo: restart automatically after an end screen.
        if ((g.state == GameState::Victory || g.state == GameState::GameOver) && g.stateTimer > 4.0f) g.startNewGame();
        bot_.drive(g, in, dt);
        return;
    }

    const FishingSystem& f = g.fishing;
    const Fish* ef = f.engagedFishPtr();
    switch (phase_) {
    case 0: // main menu + controls screen
        if (phaseTimer_ > 3.0f && !taken("01_main_menu")) shot("01_main_menu");
        else if (phaseTimer_ > 3.5f && !g.showHelp && !taken("02_controls")) g.showHelp = true;
        else if (phaseTimer_ > 4.2f && g.showHelp && !taken("02_controls")) shot("02_controls");
        else if (phaseTimer_ > 4.6f) {
            g.showHelp = false;
            phase_ = 1;
            phaseTimer_ = 0;
        }
        return;

    case 1: // a full game played by the bot, capturing every phase
        if (g.state == GameState::Aiming && !taken("03_aiming") && g.stateTimer > 0.2f) {
            in.releaseAll();
            pendingShot_ = "03_aiming";
            pendingTimer_ = 1.0f;   // hold still for a second so the camera settles
            pendingAction_ = 0;
            return;
        }
        if (g.state == GameState::Charging && f.power() > 0.55f) shot("04_charging");
        if (g.state == GameState::Casting && f.castReleased() && g.stateTimer > 0.75f) shot("05_casting");
        if (g.state == GameState::Waiting && g.stateTimer > 2.0f) shot("06_waiting");
        if (g.state == GameState::Waiting && ef && ef->mode == FishMode::Nibble) shot("07_nibble");
        if (g.state == GameState::BiteDetected && g.stateTimer > 0.15f) shot("08_bite");
        if (g.state == GameState::FishHooked && g.stateTimer > 0.35f) shot("09_fish_on");
        if (g.state == GameState::Reeling && f.tension() > 0.5f && g.caught == 0) shot("10_reeling");
        if (g.state == GameState::FishCaught && g.stateTimer > 1.2f) shot("11_caught");

        if (g.caught >= 1 && g.state == GameState::Waiting && g.stateTimer > 1.0f && !taken("12_topdown")) {
            cam.setMode(CameraMode::TopDown);
            pendingShot_ = "12_topdown";
            pendingTimer_ = 1.2f;
            pendingAction_ = 1;
            return;
        }
        if (g.caught >= 2 && g.state == GameState::Waiting && g.stateTimer > 1.0f && !taken("13_bobber_cam")) {
            cam.setMode(CameraMode::Bobber);
            pendingShot_ = "13_bobber_cam";
            pendingTimer_ = 1.5f;
            pendingAction_ = 1;
            return;
        }
        if (g.caught >= 2 && g.state == GameState::Reeling && g.stateTimer > 1.5f && !taken("14_pause")) {
            in.keyDown('p'); in.keyUp('p');
            pendingShot_ = "14_pause";
            pendingTimer_ = 0.5f;
            pendingAction_ = 2;
            return;
        }
        if (g.caught >= 3 && g.state == GameState::Reeling && f.tension() > 0.55f) shot("15_reeling_late");
        if (g.state == GameState::Victory && g.stateTimer > 1.5f) {
            shot("16_victory");
            postAction_ = 3; // press ENTER (Play Again) after the shot frame
            phase_ = 2;
            phaseTimer_ = 0;
            bot_.opt.reelAlways = true;
            return;
        }
        if (g.state == GameState::GameOver) { log("unexpected game over: %s", g.endReason.c_str()); phase_ = 2; }
        break;

    case 2: // provoke a line break, then let the sun set
        if (g.state == GameState::LineBroken && g.stateTimer > 0.25f) {
            shot("17_line_broken");
            bot_.opt.reelAlways = false;
            bot_.opt.neverHook = true;
            g.timeLeft = std::fmin(g.timeLeft, 32.0f); // jump to the end of the day
            phase_ = 3;
        }
        if (phaseTimer_ > 240.0f) phase_ = 3;
        break;

    case 3:
        if (g.state == GameState::Waiting && g.stateTimer > 1.5f && g.timeLeft < 20.0f) shot("18_sunset");
        if (g.state == GameState::GameOver && g.stateTimer > 1.5f) {
            shot("19_game_over");
            phase_ = 4;
            phaseTimer_ = 0;
        }
        break;

    case 4:
        if (phaseTimer_ > 0.5f) {
            log("demo finished: all screenshots captured (%d)", int(taken_.size()));
            finished_ = true;
        }
        return;
    }
    bot_.drive(g, in, dt);
}

} // namespace lf
