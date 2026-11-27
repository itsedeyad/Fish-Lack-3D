#pragma once
#include "game/Input.h"
#include "game/Types.h"
#include "core/Settings.h"

namespace lf {
// Shared by real mouse callbacks and headless input regression tests.
struct MouseFishingInput {
    bool down = false;
    float heldSeconds = 0;

    void button(bool pressed, GameState state, bool blocked, InputState& output) {
        down = pressed && !blocked;
        heldSeconds = 0;
        if (down && (state == GameState::Waiting || state == GameState::BiteDetected)) {
            output.keyDown('e');
            output.keyUp('e');
        }
        update(0, state, blocked, output);
    }

    void update(float dt, GameState state, bool blocked, InputState& output) {
        heldSeconds += down && !blocked ? dt : 0;
        bool charge = down && !blocked && (state == GameState::Aiming || state == GameState::Charging);
        bool reel = down && !blocked && (state == GameState::Reeling || state == GameState::FishHooked ||
            state == GameState::Hooking || (state == GameState::Waiting && heldSeconds > settings::mouseRetrieveDelay));
        if (charge) output.keyDown(KEY_SPACE); else output.keyUp(KEY_SPACE);
        if (reel) output.keyDown('r'); else output.keyUp('r');
    }
};
} // namespace lf
