#include "game/AutoPlayer.h"
#include <cmath>

namespace lf {

void AutoPlayer::press(InputState& in, unsigned char k) {
    in.keyDown(k);
    in.keyUp(k);
}

void AutoPlayer::hold(InputState& in, unsigned char k, bool on) {
    if (on && !in.isHeld(k)) in.keyDown(k);
    if (!on && in.isHeld(k)) in.keyUp(k);
}

void AutoPlayer::drive(const Game& g, InputState& in, float dt) {
    if(g.overlay==Overlay::TripSetup || g.overlay==Overlay::DailyOrder){press(in,KEY_ENTER);return;}
    const FishingSystem& f = g.fishing;
    auto releaseAim = [&] {
        hold(in, 'a', false); hold(in, 'd', false);
        hold(in, 'w', false); hold(in, 's', false);
    };

    switch (g.state) {
    case GameState::MainMenu:
        menuCooldown_ -= dt;
        if (opt.startFromMenu && menuCooldown_ <= 0) {
            if (g.menuIndex == 0) press(in, KEY_ENTER);
            else press(in, 'w');
            menuCooldown_ = 0.2f;
        }
        break;

    case GameState::Aiming: {
        hold(in, 'r', false);
        if (!hasTarget_) {
            targetYaw_ = rng_.range(-0.45f, 0.45f);
            targetPitch_ = deg2rad(rng_.range(38.0f, 50.0f));
            targetPower_ = rng_.range(0.45f, 0.95f);
            hasTarget_ = true;
        }
        float dy = targetYaw_ - f.aimYaw();
        float dp = targetPitch_ - f.aimPitch();
        hold(in, 'a', dy > 0.03f);
        hold(in, 'd', dy < -0.03f);
        hold(in, 'w', dp > 0.03f);
        hold(in, 's', dp < -0.03f);
        if (std::fabs(dy) <= 0.03f && std::fabs(dp) <= 0.03f) {
            releaseAim();
            hold(in, KEY_SPACE, true);
        }
        break;
    }

    case GameState::FishCaught:
        hold(in, KEY_SPACE, false); hold(in, 'r', false);
        press(in, KEY_ENTER);
        break;

    case GameState::Charging:
        releaseAim();
        hold(in, KEY_SPACE, true);
        if (std::fabs(f.power() - targetPower_) < 0.04f) {
            hold(in, KEY_SPACE, false);
            hasTarget_ = false;
        }
        break;

    case GameState::Waiting:
        releaseAim();
        hold(in, KEY_SPACE, false);
        hold(in, 'r', false);
        if (opt.hookEarly) {
            const Fish* fish = f.engagedFishPtr();
            if (fish && fish->mode == FishMode::Nibble) press(in, 'e');
        }
        reactTimer_ = 0;
        break;

    case GameState::BiteDetected:
        reactTimer_ += dt;
        if (!opt.neverHook && reactTimer_ >= opt.reaction) {
            press(in, 'e');
            reactTimer_ = 0;
        }
        break;

    case GameState::Reeling: {
        if (opt.reelAlways) { hold(in, 'r', true); break; }
        if (opt.neverReel) { hold(in, 'r', false); break; }
        float t = f.tension();
        bool surge = f.fightPhase() == FightPhase::Surge;
        bool warn = f.fightPhase() == FightPhase::Warn;
        if (t > 0.60f || (surge && t > 0.20f) || warn) hold(in, 'r', false);
        else if ((t < 0.35f && !surge) || t < 0.08f) hold(in, 'r', true);
        break;
    }

    default:
        releaseAim();
        hold(in, KEY_SPACE, false);
        hold(in, 'r', false);
        reactTimer_ = 0;
        break;
    }
}

} // namespace lf
