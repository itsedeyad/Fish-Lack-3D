#include "render/Camera.h"
#include "core/ViewMath.h"
#include "render/GLHeaders.h"
#include "core/World.h"
#include "game/Game.h"
#include "core/Settings.h"
#include <cmath>
#include <initializer_list>

namespace lf {

const char* Camera::modeName() const {
    switch (mode_) {
    case CameraMode::Gameplay: return "Gameplay";
    case CameraMode::Bobber: return "Bobber Close-up";
    case CameraMode::TopDown: return "Top-Down Map";
    case CameraMode::FirstPerson: return "Angler View";
    }
    return "";
}

void Camera::cycleMode() { mode_ = CameraMode((int(mode_) + 1) % 4); }

void Camera::reset() {
    yawOffset_ = 0.58f;
    pitch_ = 0.72f;
    distance_ = 16.0f;
    bobberDist_ = 5.0f;
    orthoSize_ = world::LAKE_RZ+8;
    firstPersonFov_ = 58.0f;
    panOffset_ = Vec3();
}

void Camera::orbit(float dx, float dy) {
    if (mode_ == CameraMode::TopDown) {
        panOffset_.x -= dx * orthoSize_ * 0.0028f;
        panOffset_.z -= dy * orthoSize_ * 0.0028f;
        panOffset_.x = clampf(panOffset_.x, -50, 50);
        panOffset_.z = clampf(panOffset_.z, -50, 50);
        return;
    }
    yawOffset_ = mode_==CameraMode::FirstPerson?clampf(yawOffset_ - dx * .005f,-1.5f,1.5f):wrapAngle(yawOffset_ - dx * .005f);
    pitch_ = clampf(pitch_ + dy * 0.004f, 0.05f, 1.25f);
}

void Camera::zoom(float steps) {
    float f = std::pow(0.88f, steps);
    switch (mode_) {
    case CameraMode::Gameplay: distance_ = clampf(distance_ * f, 3.0f, 20.0f); break;
    case CameraMode::Bobber: bobberDist_ = clampf(bobberDist_ * f, 1.8f, 14.0f); break;
    case CameraMode::TopDown: orthoSize_ = clampf(orthoSize_ * f, 8.0f, 75.0f); break;
    case CameraMode::FirstPerson: firstPersonFov_ = clampf(firstPersonFov_ * f, 36.0f, 75.0f); break;
    }
}

float Camera::zoomPercent() const {
    switch (mode_) {
    case CameraMode::Gameplay: return 16.0f / distance_ * 100.0f;
    case CameraMode::Bobber: return 5.0f / bobberDist_ * 100.0f;
    case CameraMode::TopDown: return 32.0f / orthoSize_ * 100.0f;
    case CameraMode::FirstPerson: return 58.0f / firstPersonFov_ * 100.0f;
    }
    return 100;
}

void Camera::update(float dt, const Game& game) {
    if(map_!=int(world::activeMap())){map_=int(world::activeMap());reset();initialized_=false;}
    menu_ = game.state == GameState::MainMenu;
    const FishingSystem& f = game.fishing;
    Vec3 wantEye, wantTarget;
    float rate = 5.0f;
    GameState st = game.activeState();

    if (menu_) {
        // The dock frames the foreground, with open water for the menu's hero fish.
        menuAngle_ += dt * 0.025f;
        const Vec3 targets[]={Vec3(-2,3.2f,-8),Vec3(-10,5,-8),Vec3(3,2.4f,-7)};
        const Vec3 offsets[]={Vec3(12,4.5f,9.4f),Vec3(15,7,12),Vec3(11,4.2f,8)};
        int map=int(game.menuMap);wantTarget=targets[map];
        wantEye=world::anglerPosition()+offsets[map]+Vec3(std::sin(menuAngle_),0,0);
        rate = 2.0f;
    } else if (mode_ == CameraMode::Gameplay) {
        Vec3 a = world::anglerPosition();
        // Deliberate orbit moves the camera; rod aiming leaves its frame stable.
        float yaw = PI + yawOffset_;
        Vec3 fwd = yawDir(yaw);
        Vec3 right(-std::cos(yaw), 0, std::sin(yaw));
        Vec3 pivot = a + Vec3(0, 1.7f, 0);
        wantEye = pivot - fwd * (std::cos(pitch_) * distance_) + Vec3(0, std::sin(pitch_) * distance_, 0) - right * 0.7f;
        Vec3 base = pivot + fwd * 3.5f - Vec3(0, 1.7f, 0);
        wantTarget=base; // Manual camera orbit stays independent of rod aim.
    } else if (mode_ == CameraMode::Bobber) {
        Vec3 b = f.bobberInWater() || (st == GameState::Casting && f.castReleased()) ? f.bobberPos() : f.predictedLanding();
        if (st == GameState::FishCaught) b = f.bobberPos();
        Vec3 a = world::anglerPosition();
        float yaw = std::atan2(b.x - a.x, b.z - a.z) + yawOffset_;
        float h = 0.8f + bobberDist_ * (0.15f + pitch_ * 0.5f);
        wantEye = b - yawDir(yaw) * bobberDist_ + Vec3(0, h, 0);
        wantTarget = b + Vec3(0, 0.1f, 0);
        rate = 4.0f;
    } else if (mode_ == CameraMode::FirstPerson) {
        Vec3 a = world::anglerPosition();
        // Mouse aiming moves the rod, not the world. Only deliberate right-drag turns the view.
        float yaw = PI + (yawOffset_ - 0.58f);
        Vec3 right(-std::cos(yaw),0,std::sin(yaw));
        wantEye = a + Vec3(0,1.60f,0) - right*0.25f;
        wantTarget = wantEye + yawDir(yaw)*32 + Vec3(0,-6.5f+(0.72f-pitch_)*8,0);
        rate=5;
    } else { // top-down
        Vec3 c = Vec3(0, 0, 2.0f) + panOffset_;
        wantTarget = c;
        wantEye = c + Vec3(0, 150.0f, 0.001f);
        rate = 8.0f;
    }

    // Never let a perspective camera dip under the ground or the water.
    if (!(mode_ == CameraMode::TopDown && !menu_)) {
        float ground = std::fmax(world::terrainHeight(wantEye.x, wantEye.z), world::WATER_LEVEL);
        if (wantEye.y < ground + 0.7f) wantEye.y = ground + 0.7f;
    }

    if (!initialized_) {
        eye_ = wantEye;
        target_ = wantTarget;
        initialized_ = true;
    } else {
        float k = damp(rate, dt);
        eye_ = lerpv(eye_, wantEye, k);
        target_ = lerpv(target_, wantTarget, k);
    }
}

void Camera::apply(int w, int h) const {
    float aspect = h > 0 ? float(w) / float(h) : 1.0f;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (isOrtho()) {
        float s = orthoSize_;
        glOrtho(-s * aspect, s * aspect, -s, s, 1.0, 400.0);  // parallel projection
    } else {
        float fov = mode_ == CameraMode::Bobber && !menu_ ? 45.0f : 55.0f;
        if (mode_ == CameraMode::FirstPerson && !menu_) fov = firstPersonFov_;
        float nearPlane = 0.15f, top = nearPlane * std::tan(deg2rad(fov) * 0.5f);
        glFrustum(-top * aspect, top * aspect, -top, top, nearPlane, 950.0);
    }
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Mat4 view = viewMatrix(eye_, target_, isOrtho() ? Vec3(0, 0, -1) : Vec3(0, 1, 0));
    glMultMatrixf(view.m);
}

} // namespace lf
