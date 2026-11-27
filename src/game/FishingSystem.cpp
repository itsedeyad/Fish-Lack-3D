#include "game/FishingSystem.h"
#include "core/World.h"
#include <cmath>

namespace lf {

using world::anglerPosition;

// ===================================================================== rod ==
// Hierarchical transform chain of the rod (all matrices are world space):
//   base    = T(angler) * Ry(yaw) * T(hand offset) * Rx(-elevation)
//   seg[0]  = base * T(0,0,handle) * Rx(bend0)
//   seg[i]  = seg[i-1] * T(0,0,segLen) * Rx(bend_i)
// Each blank segment bends a bit more than the previous one, so a single
// "bend" value produces a smooth curve that is strongest near the tip.
static float segmentBend(float bend, int i) { return bend * (0.06f + 0.13f * i); }

Mat4 FishingSystem::rodBaseMatrix() const {
    // Both yaw and pitch rotate around the palm, rather than orbiting the handle butt.
    return Mat4::translate(gripPivot()) * Mat4::rotateY(rodYaw_) *
           Mat4::rotateX(-rodElev_) * Mat4::translate(0,0,-.18f);
}

Mat4 FishingSystem::rodSegmentMatrix(int i) const {
    Mat4 m = rodBaseMatrix() * Mat4::translate(0, 0, ROD_HANDLE_LEN);
    for (int j = 0; j < i; ++j) m = m * Mat4::rotateX(segmentBend(rodBend_, j)) * Mat4::translate(0, 0, ROD_SEG_LEN);
    return m * Mat4::rotateX(segmentBend(rodBend_, i));
}

Vec3 FishingSystem::rodTip() const {
    return rodSegmentMatrix(ROD_SEGMENTS - 1).transformPoint(Vec3(0, 0, ROD_SEG_LEN));
}

Vec3 FishingSystem::handPosition() const {
    return rodBaseMatrix().transformPoint(Vec3(0, 0, 0.18f));
}

// ================================================================== setup ==
void FishingSystem::reset() {
    guideYaw_=guidePitch_=0;
    aimYaw_ = 0;
    aimPitch_ = deg2rad(42);
    rodYaw_ = worldYaw();
    rodElev_ = deg2rad(32);
    rodElevVel_ = 0;
    rodBend_ = 0.05f;
    resetForNextCast();
}

void FishingSystem::resetForNextCast() {
    guideYaw_=guidePitch_=0;
    power_ = 0; powerDir_ = 1;
    released_ = false;
    bobberInWater_ = false;
    bobberVisible_ = true;
    lineVisible_ = true;
    bobberDip_ = dipTarget_ = twitch_ = 0;
    engaged_ = -1;
    tension_ = 0;
    slack_ = 0;
    waitTime_ = 0;
    lineSlack_ = 0;
}

// ================================================================= aiming ==
void FishingSystem::updateAim(float dt, const FishingControls& c) {
    float yawInput = (c.left ? 1.0f : 0.0f) - (c.right ? 1.0f : 0.0f);
    float pitchInput = (c.up ? 1.0f : 0.0f) - (c.down ? 1.0f : 0.0f);
    aimYaw_ = clampf(aimYaw_ + yawInput * 0.9f * dt, deg2rad(-30), deg2rad(30));
    aimPitch_ = clampf(aimPitch_ + pitchInput * 0.6f * dt, deg2rad(20), deg2rad(65));
}

void FishingSystem::aimTowards(float yaw, float pitch, float dt) {
    yaw = clampf(yaw, deg2rad(-30), deg2rad(30));
    pitch = clampf(pitch, deg2rad(20), deg2rad(65));
    // Exponential smoothing: fast when far from the target, gentle near it.
    aimYaw_ = lerpf(aimYaw_, yaw, damp(6.0f, dt));
    aimPitch_ = lerpf(aimPitch_, pitch, damp(6.0f, dt));
}

void FishingSystem::beginCharge() { power_ = 0; powerDir_ = 1; }

void FishingSystem::updateCharge(float dt) {
    // The power meter "ping-pongs" between 0 and 1: releasing at the right
    // moment is the skill element of the cast.
    power_ += powerDir_ * 0.85f * dt;
    if (power_ > 1) { power_ = 2 - power_; powerDir_ = -1; }
    if (power_ < 0) { power_ = -power_; powerDir_ = 1; }
}

float FishingSystem::previewDistance(float power) const {
    if (power < 0) power = power_;
    // Projectile-like range: best at 45 degrees (sin 2*theta == 1).
    return 6.0f + 16.0f * power * (0.55f + 0.45f * std::sin(2.0f * aimPitch_));
}

Vec3 FishingSystem::clampToWater(const Vec3& from, Vec3 target) const {
    Vec3 dir = target - from;
    dir.y = 0;
    float len = dir.lengthXZ();
    dir = dir / (len > 1e-4f ? len : 1.0f);
    // Walk back towards the angler until the point is open water.
    for (float d = len; d > 4.0f; d -= 0.5f) {
        Vec3 p = from + dir * d;
        if (world::isWater(p.x, p.z, 0.6f) && !world::insidePier(p.x, p.z, 1.5f)) return p;
    }
    return from + dir * 6.0f;
}

Vec3 FishingSystem::predictedLanding(float power) const {
    Vec3 a = anglerPosition();
    Vec3 t = clampToWater(a, a + yawDir(worldYaw()) * previewDistance(power));
    t.y = world::WATER_LEVEL;
    return t;
}

Vec3 FishingSystem::previewArcPoint(float u, float power) const {
    Vec3 start = rodTip();
    Vec3 end = predictedLanding(power);
    float d = distXZ(anglerPosition(), end);
    float apex = clampf(1.5f + d * std::tan(aimPitch_) * 0.22f, 1.5f, 12.0f);
    return lerpv(start, end, u) + Vec3(0, 4.0f * apex * u * (1.0f - u), 0);
}

// ================================================================ casting ==
void FishingSystem::beginCast() {
    Vec3 a = anglerPosition();
    castTarget_ = predictedLanding();
    float d = distXZ(a, castTarget_);
    castApex_ = clampf(1.5f + d * std::tan(aimPitch_) * 0.22f, 1.5f, 12.0f);
    castDuration_ = 0.55f + 0.035f * d + 0.5f * std::sin(aimPitch_);
    castTimer_ = 0;
    released_ = false;
    bobberInWater_ = false;
}

Outcome FishingSystem::updateCast(float dt) {
    castTimer_ += dt;
    if (!released_) {
        // Wind-up: the rod swings back over the shoulder, then whips forward.
        constexpr float windupSeconds=.30f;
        if (castTimer_ >= windupSeconds) {
            released_ = true;
            castStart_ = rodTip();
            castTimer_ = 0;
            bobber_ = castStart_;
        }
        return Outcome::None;
    }
    reelAngle_ -= 30.0f * dt; // line pays out: reel spins backwards
    float u = castTimer_ / castDuration_;
    if (u >= 1.0f) {
        bobberRest_ = castTarget_;
        bobber_ = castTarget_;
        bobberInWater_ = true;
        engaged_ = -1;
        waitTime_ = 0;
        searchTimer_ = searchInterval_;
        twitch_ = 1.0f;
        fx_->addSplash(castTarget_, 6, .55f, *rng_);
        fx_->addRipple(castTarget_, 3.0f, 2.2f, 1.0f);
        fx_->addRipple(castTarget_, 1.6f, 1.4f, 0.8f);
        return Outcome::Landed;
    }
    // Parabolic arc: straight-line interpolation plus a parabola 4h*u*(1-u).
    bobber_ = lerpv(castStart_, castTarget_, u) + Vec3(0, 4.0f * castApex_ * u * (1.0f - u), 0);
    return Outcome::None;
}

// ================================================================ waiting ==
Outcome FishingSystem::updateWaiting(float dt, const FishingControls& c, float time) {
    (void)time;
    waitTime_ += dt;
    school_->bobberTarget = Vec3(bobber_.x, 0, bobber_.z);
    Vec3 a = anglerPosition();

    // Hold R with nothing on the line: retrieve the bobber to cast again.
    if (c.reelHeld) {
        if (engaged_ >= 0) { school_->flee(engaged_, bobber_); engaged_ = -1; }
        Vec3 to = Vec3(a.x - bobberRest_.x, 0, a.z - bobberRest_.z);
        float dist = to.lengthXZ();
        if (dist < 3.5f) { bobberInWater_ = false; return Outcome::Retrieved; }
        bobberRest_ += to / dist * (5.0f * dt);
        reelAngle_ += 16.0f * dt;
        twitch_ = std::fmax(twitch_, 0.3f);
        splashTimer_ -= dt;
        if (splashTimer_ <= 0) { fx_->addRipple(bobber_, 1.0f, 1.0f, 0.4f); splashTimer_ = 0.35f; }
        waitTime_ = 0;
        return Outcome::None;
    }

    if (c.hookPressed) {
        if (engaged_ >= 0) {
            FishMode m = school_->fish[engaged_].mode;
            if (m == FishMode::Nibble || m == FishMode::Approach) {
                school_->flee(engaged_, bobber_);
                engaged_ = -1;
                twitch_ = 1.0f;
                fx_->addSplash(bobber_, 3, .4f, *rng_);
                return Outcome::Spooked;
            }
        }
        twitch_ = 0.6f;
        return Outcome::NothingBiting;
    }

    if (engaged_ < 0) {
        // Look for an interested fish. The search radius and the chance grow
        // the longer the bobber sits still, so a bite always comes eventually.
        searchTimer_ -= dt;
        if (searchTimer_ <= 0) {
            searchTimer_ = 0.8f * searchInterval_;
            float radius = 10.0f + waitTime_ * 1.5f;
            int cand = school_->findCandidate(bobber_, radius*gear_.searchRadius,gear_.preferredA,gear_.preferredB);
            float chance = 0.30f + waitTime_ * 0.06f;
            if (cand < 0 && waitTime_ > 10.0f) cand = school_->findCandidate(bobber_, 500.0f,gear_.preferredA,gear_.preferredB);
            if (cand >= 0 && (rng_->chance(chance) || waitTime_ > 8.0f)) {
                engaged_ = cand;
                school_->startApproach(cand);
            }
        }
        return Outcome::None;
    }

    Fish& f = school_->fish[engaged_];
    if (f.mode == FishMode::Approach) {
        if (distXZ(f.pos, bobber_) < 1.3f) {
            school_->startNibble(engaged_);
            nibblesLeft_ = rng_->irange(1, 3);
            nibbleTimer_ = rng_->range(0.8f, 1.5f);
        } else if (f.modeTimer > 30.0f) {
            school_->flee(engaged_, bobber_);
            engaged_ = -1;
        }
    } else if (f.mode == FishMode::Nibble) {
        nibbleTimer_ -= dt;
        if (nibbleTimer_ <= 0) {
            if (nibblesLeft_ > 0) {
                // A nibble: small twitch of the bobber - NOT the real bite.
                --nibblesLeft_;
                twitch_ = 1.0f;
                fx_->addRipple(bobber_, 0.9f, 1.1f, 0.5f);
                nibbleTimer_ = rng_->range(1.0f, 1.8f);
            } else {
                // The real bite: bobber is dragged under.
                school_->startBite(engaged_);
                biteWindow_ = fishSpec(f.type).biteWindow * biteMul_;
                biteTimer_ = biteWindow_;
                biteClock_ = 0;
                dipTarget_ = 0.32f;
                twitch_ = 1.0f;
                fx_->addSplash(bobber_, 4, .45f, *rng_);
                fx_->addRipple(bobber_, 2.2f, 1.5f, 1.0f);
                return Outcome::Bite;
            }
        }
    } else {
        engaged_ = -1; // fish was interrupted
    }
    return Outcome::None;
}

// =================================================================== bite ==
Outcome FishingSystem::updateBite(float dt, const FishingControls& c) {
    biteTimer_ -= dt;
    biteClock_ += dt;
    dipTarget_ = 0.30f + 0.08f * std::sin(biteClock_ * 22.0f);
    if (int(biteClock_ * 6) % 2 == 0 && rng_->chance(dt * 6)) fx_->addRipple(bobber_, 1.2f, 0.9f, 0.6f);
    // Resolve this tick's hook press before the existing timeout check.
    if (c.hookPressed) {
        float reaction = biteWindow_ - biteTimer_;
        perfectHook_ = reaction < biteWindow_ * 0.4f;
        return Outcome::Hooked;
    }
    if (biteTimer_ <= 0) {
        biteTimer_ = 0;
        if (engaged_ >= 0) school_->flee(engaged_, bobber_);
        engaged_ = -1;
        dipTarget_ = 0;
        return Outcome::BiteMissed;
    }
    return Outcome::None;
}

void FishingSystem::beginHook() {
    twitch_ = 1.0f;
    fx_->addSplash(bobber_, 6, .6f, *rng_);
    fx_->addRipple(bobber_, 2.5f, 1.6f, 1.0f);
    if (engaged_ >= 0) school_->setHooked(engaged_);
    // Remember the fight geometry now so the hooked preview can use it.
    Vec3 a = anglerPosition();
    lineLen_ = distXZ(a, bobber_);
    fightYaw_ = std::atan2(bobber_.x - a.x, bobber_.z - a.z);
    sway_ = swayTarget_ = 0;
    fightPhase_ = FightPhase::Surge;
}

// ================================================================== fight ==
void FishingSystem::beginFight(float strengthMul) {
    if (engaged_ < 0) return;
    const FishSpec& sp = fishSpec(school_->fish[engaged_].type);
    Vec3 a = anglerPosition();
    lineLen_ = distXZ(a, bobber_);
    fightYaw_ = std::atan2(bobber_.x - a.x, bobber_.z - a.z);
    tension_ = 0.25f;quality_=100;
    maxStamina_ = stamina_ = sp.stamina * strengthMul;
    strength_ = sp.strength * strengthMul;
    fightPhase_ = FightPhase::Rest;
    fightTimer_ = rng_->range(1.0f, 2.0f);
    slack_ = 0;
    splashTimer_ = 0;
}

void FishingSystem::positionHookedFish(float dt, float time) {
    if (engaged_ < 0) return;
    Fish& f = school_->fish[engaged_];
    Vec3 a = anglerPosition();
    bool surge = fightPhase_ == FightPhase::Surge;

    if (fightPhase_ == FightPhase::Rest) swayTarget_ *= (1.0f - 0.3f * dt);
    sway_ = lerpf(sway_, swayTarget_ + 0.10f * std::sin(time * 2.3f), damp(surge ? 3.5f : 1.5f, dt));

    // Fish position: on a circle of radius = line length around the angler,
    // swaying left/right as it fights.
    Vec3 p = a + yawDir(fightYaw_ + sway_) * lineLen_;
    for (int k = 0; k < 6 && !world::isWater(p.x, p.z, 0.8f); ++k) {
        float toCenter = std::atan2(-a.x, -a.z - 10.0f);
        fightYaw_ += wrapAngle(toCenter - fightYaw_) * 0.15f;
        sway_ *= 0.7f; swayTarget_ *= 0.7f;
        p = a + yawDir(fightYaw_ + sway_) * lineLen_;
    }
    float wantY = -0.30f - (surge ? 0.15f : 0.0f) + 0.07f * std::sin(time * 9.0f);
    f.pos = Vec3(p.x, lerpf(f.pos.y, wantY, damp(5.0f, dt)), p.z);
    f.yaw = fightYaw_ + sway_ + 0.45f * std::sin(time * (surge ? 11.0f : 5.0f));
    f.roll = 0.35f * std::sin(time * 7.0f);
    f.pitch = 0.0f;
    bobberRest_ = Vec3(p.x, 0, p.z);
}

Outcome FishingSystem::updateFight(float dt, const FishingControls& c, float time, float tensionMul) {
    if (engaged_ < 0) return Outcome::EscapedSlack;
    float ratio = fishStaminaRatio();

    // Fish behaviour cycle: Rest -> Warn (telegraph) -> Surge (hard pull).
    fightTimer_ -= dt;
    if (fightTimer_ <= 0) {
        switch (fightPhase_) {
        case FightPhase::Rest:
            fightPhase_ = FightPhase::Warn;
            fightTimer_ = 0.5f;
            break;
        case FightPhase::Warn:
            fightPhase_ = FightPhase::Surge;
            fightTimer_ = rng_->range(0.7f, 1.5f) * (0.5f + 0.5f * ratio);
            swayTarget_ = clampf(sway_ + (rng_->chance(0.5f) ? 1.0f : -1.0f) * rng_->range(0.2f, 0.4f), -0.6f, 0.6f);
            break;
        case FightPhase::Surge:
            fightPhase_ = FightPhase::Rest;
            fightTimer_ = rng_->range(1.0f, 2.4f) * (1.4f - 0.4f * ratio);
            break;
        }
    }
    bool surge = fightPhase_ == FightPhase::Surge;
    float phaseMul = surge ? 1.5f : (fightPhase_ == FightPhase::Warn ? 0.5f : 0.25f);
    float pull = strength_ * phaseMul * (0.4f + 0.6f * ratio);

    // Tension model (simple, tunable rates per second):
    //   reeling     -> tension rises (faster when the fish pulls), line shortens
    //   not reeling -> tension relaxes, but the fish takes some line back
    if (c.reelHeld) {
        tension_ += (0.20f + pull * 0.75f) * tensionMul * dt;
        lineLen_ -= 3.0f * gear_.reel * (surge ? 0.35f : 1.0f) * dt;
        reelAngle_ += 16.0f * dt;
    } else {
        tension_ += (pull * 0.55f * tensionMul - 0.42f) * dt;
        lineLen_ += pull * (surge ? 2.2f : 0.4f) * dt;
        reelAngle_ -= pull * 3.0f * dt;
    }
    tension_ = std::fmax(0.0f, tension_);
    quality_=catchQualityAfter(quality_,dt*gear_.qualityLoss,c.reelHeld,tension_,surge);
    stamina_ = std::fmax(0.0f, stamina_ - dt * (0.35f + tension_ * 1.1f));

    if (tension_ < 0.04f) slack_ += dt;
    else slack_ = std::fmax(0.0f, slack_ - dt * 2.0f);

    positionHookedFish(dt, time);

    splashTimer_ -= dt;
    if (splashTimer_ <= 0) {
        Fish& f = school_->fish[engaged_];
        Vec3 s(f.pos.x, 0.0f, f.pos.z);
        fx_->addSplash(s, surge ? 4 : 2, surge ? 0.5f : 0.3f, *rng_);
        fx_->addRipple(s, surge ? 2.0f : 1.2f, 1.2f, surge ? 0.9f : 0.5f);
        splashTimer_ = surge ? 0.25f : 0.8f;
    }
    dipTarget_ = 0.04f + (surge ? 0.14f : 0.0f);

    if (tension_ >= 1.0f) { tension_ = 1.0f; return Outcome::LineSnapped; }
    if (lineLen_ > gear_.maxLine) return Outcome::EscapedRun;
    if (slack_ > 2.3f) return Outcome::EscapedSlack;
    if (lineLen_ <= CATCH_DISTANCE) return Outcome::Caught;
    return Outcome::None;
}

// ================================================================ landing ==
void FishingSystem::beginLanding() {
    if (engaged_ < 0) return;
    school_->startLanding(engaged_);
    landStart_ = school_->fish[engaged_].pos;
    fx_->addSplash(Vec3(landStart_.x, 0, landStart_.z), 6, .6f, *rng_);
    fx_->addRipple(Vec3(landStart_.x, 0, landStart_.z), 3.0f, 2.0f, 1.0f);
    bobberInWater_ = false;
    tension_ = 0.4f;
}

void FishingSystem::releaseCatch() {
    if(engaged_>=0) {
        school_->fish[engaged_].pos=Vec3(landStart_.x,-0.8f,landStart_.z);
        school_->flee(engaged_,anglerPosition());
    }
    engaged_=-1;
}

void FishingSystem::finishCatch() {
    if (engaged_ >= 0) school_->deactivate(engaged_);
    engaged_ = -1;
}

const Fish* FishingSystem::engagedFishPtr() const {
    if (engaged_ < 0 || !school_) return nullptr;
    return &school_->fish[engaged_];
}

void FishingSystem::loseFish() {
    if (engaged_ >= 0) school_->flee(engaged_, anglerPosition());
    engaged_ = -1;
    tension_ = 0;
}

void FishingSystem::updateResult(float dt) {
    // After an escape the empty bobber is reeled back in; after a snap the
    // line is gone and the lost bobber drifts away.
    dipTarget_ = 0;
    if (!bobberInWater_) return;
    Vec3 a = anglerPosition();
    Vec3 to = Vec3(a.x - bobberRest_.x, 0, a.z - bobberRest_.z);
    float dist = to.lengthXZ();
    if (lineVisible_) {
        if (dist > 3.5f) { bobberRest_ += to / dist * (6.0f * dt); reelAngle_ += 18.0f * dt; }
        else bobberInWater_ = false;
    } else {
        bobberRest_ -= to / std::fmax(dist, 0.1f) * (0.4f * dt);
    }
}

// ============================================================== animation ==
void FishingSystem::updateAnimation(float dt, float time, GameState state) {
    float targetElev = 32, targetBend = 0.04f, stiffness = 90.0f;
    float yawTarget = worldYaw();
    bool caughtHang = false;

    switch (state) {
    case GameState::Aiming:
        targetElev = rad2deg(aimPitch_);
        break;
    case GameState::Charging:
        targetElev = std::min(72.f,rad2deg(aimPitch_)+power_*8);
        targetBend = 0.08f + power_ * 0.1f;
        break;
    case GameState::Casting:
        if (!released_) { targetElev = 115; targetBend = 0.15f; stiffness = 160; }
        else { targetElev = 18; targetBend = 0.05f; stiffness = 170; }
        break;
    case GameState::Waiting:
        targetElev = 26;
        targetBend = 0.05f + bobberDip_ * 0.4f;
        break;
    case GameState::BiteDetected:
        targetElev = 24; targetBend = 0.35f;
        break;
    case GameState::Hooking:
        targetElev = 78; targetBend = 0.7f; stiffness = 220;
        break;
    case GameState::FishHooked:
    case GameState::Reeling: {
        targetElev = 48 + tension_ * 14;
        targetBend = 0.15f + tension_ * 0.85f;
        Vec3 a = anglerPosition();
        float fishYaw = std::atan2(bobber_.x - a.x, bobber_.z - a.z);
        yawTarget = fishYaw + guideYaw_;
        stiffness = 120;
        break;
    }
    case GameState::FishCaught:
        targetElev = 72; targetBend = 0.45f;
        caughtHang = true;
        break;
    case GameState::FishEscaped:
    case GameState::LineBroken:
        targetElev = 30; targetBend = 0.0f;
        break;
    default:
        break;
    }

    if(state==GameState::Waiting || state==GameState::BiteDetected || state==GameState::Reeling || state==GameState::FishHooked) {
        targetElev+=rad2deg(guidePitch_);
        if(state==GameState::Waiting || state==GameState::BiteDetected)yawTarget+=guideYaw_;
    }

    // Rod elevation: an under-damped spring so the rod overshoots slightly,
    // which reads as a flexible "whip" during the cast and the hook strike.
    float target = deg2rad(targetElev);
    float damping = 2.0f * std::sqrt(stiffness) * 0.55f;
    rodElevVel_ += ((target - rodElev_) * stiffness - rodElevVel_ * damping) * dt;
    rodElev_ += rodElevVel_ * dt;
    rodBend_ = lerpf(rodBend_, targetBend, damp(8.0f, dt));
    rodYaw_ = wrapAngle(rodYaw_ + wrapAngle(yawTarget - rodYaw_) * damp(6.0f, dt));

    // Bobber
    twitch_ = std::fmax(0.0f, twitch_ - dt * 2.5f);
    bobberDip_ = lerpf(bobberDip_, dipTarget_ + twitch_ * 0.12f * std::fabs(std::sin(time * 18.0f)), damp(10.0f, dt));

    if (bobberInWater_) {
        bobber_.x = bobberRest_.x;
        bobber_.z = bobberRest_.z;
        bobber_.y = world::waveHeight(bobber_.x, bobber_.z, time) + 0.03f - bobberDip_;
        Vec3 n = world::waveNormal(bobber_.x, bobber_.z, time);
        n.x += twitch_ * 0.35f * std::sin(time * 23.0f);
        n.z += twitch_ * 0.35f * std::cos(time * 19.0f);
        if (state == GameState::Reeling || state == GameState::FishHooked) {
            // The bobber leans towards the angler: the line is pulling it.
            Vec3 a = anglerPosition();
            Vec3 toA = Vec3(a.x - bobber_.x, 0, a.z - bobber_.z).normalized();
            n += toA * (0.3f + tension_ * 0.6f);
        }
        bobberTilt_ = n.normalized();
    } else if (!(state == GameState::Casting && released_) && !caughtHang) {
        // Bobber dangles under the rod tip.
        Vec3 tip = rodTip();
        bobber_ = tip + Vec3(0.06f * std::sin(time * 1.7f), -0.75f, 0.06f * std::cos(time * 1.3f));
        bobberTilt_ = Vec3(0, 1, 0);
    }

    // Line sag: a slack line hangs in a curve, a tight one is straight.
    float wantSlack = 0.0f;
    switch (state) {
    case GameState::Waiting: wantSlack = 0.30f; break;
    case GameState::BiteDetected: wantSlack = 0.2f; break;
    case GameState::Casting: wantSlack = released_ ? 0.25f : 0.0f; break;
    case GameState::FishHooked: case GameState::Reeling: wantSlack = clampf(0.8f - tension_ * 1.1f, 0.0f, 0.8f); break;
    case GameState::FishEscaped: wantSlack = 0.6f; break;
    default: wantSlack = 0.0f; break;
    }
    lineSlack_ = lerpf(lineSlack_, wantSlack, damp(5.0f, dt));
}

Vec3 FishingSystem::gripPivot() const {
    return anglerPosition()+Vec3(.20f,1.30f,-.70f);
}
void FishingSystem::guideRod(float yaw,float pitch,float dt) {
    guideYaw_=lerpf(guideYaw_,clampf(yaw,deg2rad(-12),deg2rad(12)),damp(6,dt));
    guidePitch_=lerpf(guidePitch_,clampf(pitch,deg2rad(-15),deg2rad(15)),damp(6,dt));
}
} // namespace lf
