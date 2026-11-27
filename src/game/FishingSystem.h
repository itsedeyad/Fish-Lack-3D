// =============================================================================
//  FishingSystem.h - the fishing mechanics: aiming, casting, the bobber,
//  bites, hooking, the reeling/tension mini-game and the fishing rod pose.
//
//  The Game state machine decides WHICH phase is active and calls the matching
//  update function here; each one returns an Outcome describing what happened
//  (e.g. the bobber landed, a fish bit, the line snapped). Game reacts to the
//  outcome by switching state. This keeps the state machine in one place and
//  the mechanics in another.
//
//  The rod is a hierarchical model: base -> handle -> 4 flexible blank
//  segments. Its transform chain is computed here with Mat4 (on the CPU) so
//  the logic knows the exact rod-tip position for the line and the bobber;
//  the renderer draws with the same matrices.
// =============================================================================
#pragma once
#include "core/MathUtil.h"
#include "game/Types.h"
#include "game/Fish.h"
#include "game/Effects.h"
#include "game/Session.h"

namespace lf {

struct FishingControls {
    bool left = false, right = false, up = false, down = false;
    bool hookPressed = false;
    bool reelHeld = false;
};

enum class Outcome {
    None,
    Landed,          // bobber touched the water
    NothingBiting,   // hook pressed with no fish around
    Spooked,         // hook pressed too early (fish was only nibbling)
    Bite,            // a fish took the bait
    BiteMissed,      // hook window expired
    Hooked,          // successful hook
    Retrieved,       // bobber reeled back without a fish
    Caught,          // fish landed
    EscapedSlack,    // line was slack too long
    EscapedRun,      // fish took all the line
    LineSnapped,     // tension reached 100 %
};

enum class FightPhase { Rest, Warn, Surge };

inline float catchQualityAfter(float quality,float dt,bool reeling,float tension,bool surge) {
    float loss=reeling?((tension>=.8f?8.f:0.f)+(surge?4.f:0.f)):0.f;
    return clampf(quality-std::max(0.f,dt)*loss,40,100);
}

class FishingSystem {
public:
    static constexpr int   ROD_SEGMENTS = 4;
    static constexpr float ROD_HANDLE_LEN = 0.50f;
    static constexpr float ROD_SEG_LEN = 0.62f;
    static constexpr float MAX_LINE = 46.0f;
    static constexpr float CATCH_DISTANCE = 2.4f;

    void attach(FishSchool* school, Effects* fx, Rng* rng) { school_ = school; fx_ = fx; rng_ = rng; }
    void setLoadout(const LoadoutStats& value) { gear_=value;searchInterval_=value.search; }
    void guideRod(float yaw,float pitch,float dt);
    Vec3 gripPivot() const;
    float guideYaw() const {return guideYaw_;}
    float guidePitch() const {return guidePitch_;}
    void setSearchInterval(float factor) { searchInterval_=factor; }
    void setBiteWindowMul(float m) { biteMul_ = m; }
    void reset();                 // new game
    void resetForNextCast();      // back to the aiming pose

    // ---- per-phase updates (called by Game according to its state) ----
    void   updateAim(float dt, const FishingControls& c);
    /// Mouse aiming: turn smoothly towards the requested direction / angle.
    void   aimTowards(float yaw, float pitch, float dt);
    void   beginCharge();
    void   updateCharge(float dt);
    void   beginCast();
    Outcome updateCast(float dt);
    Outcome updateWaiting(float dt, const FishingControls& c, float time);
    Outcome updateBite(float dt, const FishingControls& c);
    void   beginHook();
    void   beginFight(float strengthMul);
    Outcome updateFight(float dt, const FishingControls& c, float time, float tensionMul);
    void   beginLanding();
    void   releaseCatch();        // return the fish to the lake, without inventory
    void   finishCatch();         // remove the landed fish from the lake
    void   loseFish();            // escape / snap: fish flees
    void   snapLine() { lineVisible_ = false; bobberInWater_ = true; }
    void   updateResult(float dt);// bobber drifts back after a lost fish

    /// Animation that runs in every playing state: rod pose spring, reel,
    /// bobber floating on the waves. `state` selects the target pose.
    void updateAnimation(float dt, float time, GameState state);

    // ---- queries for the renderer / HUD / AutoPlayer ----
    float aimYaw() const { return aimYaw_; }
    float aimPitch() const { return aimPitch_; }
    float worldYaw() const { return PI + aimYaw_; }
    float power() const { return power_; }
    float tension() const { return tension_; }
    int fishQuality() const {return int(std::round(quality_));}
    float lineLength() const { return lineLen_; }
    float biteTimeLeft() const { return biteTimer_; }
    float biteWindow() const { return biteWindow_; }
    bool  lastHookPerfect() const { return perfectHook_; }
    FightPhase fightPhase() const { return fightPhase_; }
    float fishStaminaRatio() const { return maxStamina_ > 0 ? stamina_ / maxStamina_ : 0.0f; }
    int   engagedFish() const { return engaged_; }
    const Fish* engagedFishPtr() const;
    float reelAngle() const { return reelAngle_; }
    float rodBend() const { return rodBend_; }
    float slackTimer() const { return slack_; }
    /// Predicted cast distance / landing / arc. power < 0 = current power.
    float previewDistance(float power = -1.0f) const;
    Vec3  predictedLanding(float power = -1.0f) const;
    Vec3  previewArcPoint(float u, float power = -1.0f) const;
    bool  castReleased() const { return released_; }
    bool  showAimMarker() const { return true; }

    Vec3  bobberPos() const { return bobber_; }
    Vec3  bobberTilt() const { return bobberTilt_; }
    bool  bobberVisible() const { return bobberVisible_; }
    bool  bobberInWater() const { return bobberInWater_; }
    float bobberSubmerge() const { return bobberDip_; }
    float lineSlack() const { return lineSlack_; }
    bool  lineVisible() const { return lineVisible_; }
    Vec3  lineEnd() const { return bobber_; }

    float anglerYaw() const { return rodYaw_; }
    Mat4  rodBaseMatrix() const;            // world matrix of the rod handle base
    Mat4  rodSegmentMatrix(int i) const;    // world matrix at the start of blank segment i
    Vec3  rodTip() const;
    Vec3  handPosition() const;             // where the angler's hands grip the rod

private:
    Vec3 clampToWater(const Vec3& from, Vec3 target) const;
    void positionHookedFish(float dt, float time);
    LoadoutStats gear_;
    float guideYaw_=0,guidePitch_=0;
    float searchInterval_ = 1.0f;
    float biteMul_ = 1.0f;
    float quality_=100;
    float biteClock_ = 0;
public:
    /// Keeps the hooked fish thrashing at the bobber before the fight starts.
    void updateHookedPreview(float dt, float time) { positionHookedFish(dt, time); }
private:

    FishSchool* school_ = nullptr;
    Effects* fx_ = nullptr;
    Rng* rng_ = nullptr;

    // aiming / casting
    float aimYaw_ = 0, aimPitch_ = deg2rad(42);
    float power_ = 0, powerDir_ = 1;
    float castTimer_ = 0, castDuration_ = 1, castApex_ = 3;
    bool  released_ = false;
    Vec3  castStart_, castTarget_;

    // bobber
    Vec3  bobber_, bobberTilt_{0, 1, 0};
    Vec3  bobberRest_;               // x/z where it floats
    bool  bobberVisible_ = true, bobberInWater_ = false, lineVisible_ = true;
    float bobberDip_ = 0, dipTarget_ = 0, twitch_ = 0;
    float lineSlack_ = 0.5f;

    // bite logic
    int   engaged_ = -1;
    float waitTime_ = 0, searchTimer_ = 0;
    int   nibblesLeft_ = 0;
    float nibbleTimer_ = 0;
    float biteTimer_ = 0, biteWindow_ = 1;
    bool  perfectHook_ = false;

    // fight
    float tension_ = 0, lineLen_ = 0, stamina_ = 0, maxStamina_ = 1, strength_ = 0.5f;
    float slack_ = 0, fightTimer_ = 0, fightYaw_ = 0, sway_ = 0, swayTarget_ = 0, splashTimer_ = 0;
    FightPhase fightPhase_ = FightPhase::Rest;
    Vec3  landStart_;

    // rod animation
    float rodElev_ = deg2rad(35), rodElevVel_ = 0, rodBend_ = 0.05f, rodYaw_ = PI, reelAngle_ = 0;
};

} // namespace lf
