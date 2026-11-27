// =============================================================================
//  Input.h - keyboard state shared between GLUT callbacks and the game.
//
//  GLUT reports key-down and key-up events. We store, for each key:
//   held[k]     - currently pressed
//   pressed[k]  - went down since the last update (edge)
//   released[k] - went up since the last update (edge)
//  Edge flags are cleared after each simulation step (endFrame).
//  Special keys (arrows, F-keys) are mapped to otherwise unused low codes.
// =============================================================================
#pragma once
#include <cctype>
#include <cstring>

namespace lf {

enum KeyCode : unsigned char {
    KEY_UP = 1, KEY_DOWN = 2, KEY_LEFT = 3, KEY_RIGHT = 4,
    KEY_F5 = 5, KEY_F12 = 6, KEY_F1 = 7, KEY_F11 = 11,
    KEY_ENTER = 13, KEY_ESC = 27, KEY_SPACE = 32,
};

struct InputState {
    bool held[256] = {};
    bool pressed[256] = {};
    bool released[256] = {};
    // Mouse aiming: cursor position normalised to [-1,1] (x right, y down).
    // mouseAim is set when the mouse moves and cleared by keyboard aiming.
    bool  mouseAim = false;
    float mouseX = 0, mouseY = 0;

    static unsigned char norm(unsigned char k) { return (unsigned char)std::tolower(k); }
    void keyDown(unsigned char k) {
        k = norm(k);
        if (!held[k]) pressed[k] = true;
        held[k] = true;
    }
    void keyUp(unsigned char k) {
        k = norm(k);
        if (held[k]) released[k] = true;
        held[k] = false;
    }
    bool isHeld(unsigned char k) const { return held[norm(k)]; }
    bool wasPressed(unsigned char k) const { return pressed[norm(k)]; }
    bool wasReleased(unsigned char k) const { return released[norm(k)]; }
    void consume(unsigned char k) { pressed[norm(k)] = false; }
    void endFrame() {
        std::memset(pressed, 0, sizeof(pressed));
        std::memset(released, 0, sizeof(released));
    }
    void releaseAll() {
        for (int k = 0; k < 256; ++k) if (held[k]) keyUp((unsigned char)k);
    }
};

// Each callback writes to its own source. Releasing one device must never
// release a key still held by the other. Short taps retain their press edge.
struct InputSources {
    InputState keyboard, mouse;
    void mergeInto(InputState& result) {
        for (int k = 0; k < 256; ++k) {
            if (InputState::norm((unsigned char)k) != k) continue;
            bool down = keyboard.held[k] || mouse.held[k];
            if (down) result.keyDown((unsigned char)k);
            else {
                if (!result.held[k] && (keyboard.pressed[k] || mouse.pressed[k]))
                    result.keyDown((unsigned char)k);
                result.keyUp((unsigned char)k);
            }
        }
        keyboard.endFrame();
        mouse.endFrame();
    }
};

} // namespace lf
