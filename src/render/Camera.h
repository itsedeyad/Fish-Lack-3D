// =============================================================================
//  Camera.h - four camera modes.
//    Gameplay : perspective, behind and above the angler, follows the aim and
//               shifts towards the bobber/fish. Mouse drag orbits, wheel zooms.
//    Bobber   : perspective close-up of the bobber (great for spotting bites).
//    FirstPerson: perspective from the angler, with the actual rod in the foreground.
//    TopDown  : orthographic map view of the lake (glOrtho). Drag pans.
//  The main menu uses a slow cinematic orbit around the lake.
//  Eye/target positions are smoothed with frame-rate independent damping.
// =============================================================================
#pragma once
#include "core/MathUtil.h"

namespace lf {
class Game;

enum class CameraMode { FirstPerson = 0, Bobber = 1, Gameplay = 2, TopDown = 3 };

class Camera {
public:
    void update(float dt, const Game& game);
    /// Load projection + view matrices for a window of size w x h.
    void apply(int w, int h) const;

    void cycleMode();
    void setMode(CameraMode m) { mode_ = m; }
    CameraMode mode() const { return mode_; }
    const char* modeName() const;
    bool isOrtho() const { return mode_ == CameraMode::TopDown && !menu_; }

    void orbit(float dxPixels, float dyPixels);
    void zoom(float steps);   // positive = zoom in
    void reset();

    Vec3 eye() const { return eye_; }
    Vec3 target() const { return target_; }
    float zoomPercent() const;

private:
    CameraMode mode_ = CameraMode::FirstPerson;
    bool  menu_ = true;
    float yawOffset_ = 0.58f, pitch_ = 0.72f, distance_ = 16.0f;
    float bobberDist_ = 5.0f, orthoSize_ = 32.0f, firstPersonFov_ = 58.0f;
    Vec3  panOffset_;
    Vec3  eye_{0, 6, 32}, target_{0, 1, 10};
    float menuAngle_ = 0;
    bool  initialized_ = false;
    int map_=-1;
};

} // namespace lf
