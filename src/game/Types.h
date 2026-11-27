// =============================================================================
//  Types.h - enums and data tables shared by the game logic.
// =============================================================================
#pragma once
#include "core/MathUtil.h"

namespace lf {

/// Every screen / phase of the game is one value of this enum. Game::update()
/// is a switch over it - a classic finite state machine.
enum class GameState {
    MainMenu,
    Aiming,        // playing: choose direction and angle
    Charging,      // playing: SPACE held, power meter oscillates
    Casting,       // playing: rod swing + bobber flight
    Waiting,       // playing: bobber floats, fish may approach
    BiteDetected,  // playing: bobber pulled under, hook window open
    Hooking,       // playing: rod strike animation
    FishHooked,    // playing: "FISH ON!" moment
    Reeling,       // playing: tension mini-game
    FishCaught,    // playing: result display
    FishEscaped,   // playing: result display
    LineBroken,    // playing: result display
    Paused,
    Victory,
    GameOver,
};

const char* stateName(GameState s);
/// Gameplay phases; Game handles pause and catch-choice timer freezes separately.
bool isPlayingState(GameState s);

enum class RoundMode { Classic, FullDay, Hardcore };
const char* roundModeName(RoundMode mode);
enum class DayEnd { Goal, Sunset, NoLines, Early, NoBait };
enum class SoundCue { Click, Cast, Splash, Bite, Reel, Catch, Purchase, Coins, LineBreak, DayEnd, Equip, Open, Close, Reject, Unlock, Accept, Complete, Poison, Release, Secret, NoBait, Count };

enum class Difficulty { Easy = 0, Normal = 1, Hard = 2 };
const char* difficultyName(Difficulty d);

struct DifficultySettings {
    int   targetFish;
    float dayLength;      // seconds until sunset
    int   spareLines;     // line breaks allowed
    float tensionMul;     // how fast tension builds
    float biteWindowMul;  // multiplier on the hook timing window
};
DifficultySettings difficultySettings(Difficulty d);

// ------------------------------------------------------------- fish types --
enum FishTypeId { FISH_BLUE = 0, FISH_TROUT = 1, FISH_CARP = 2, FISH_PERCH = 3, FISH_CATFISH = 4, FISH_PIKE = 5, FISH_TYPE_COUNT = 6 };

struct FishSpec {
    const char* name;
    float length;      // metres (stylised, larger than life to read well)
    float speed;       // cruise speed m/s
    float turnRate;    // rad/s
    float biteWindow;  // seconds to react after the bite
    float strength;    // how hard it pulls when hooked (0..1)
    float stamina;     // seconds of fight before tiring
    float depth;       // preferred swimming depth below the surface
    Color back, belly, accent;
};
const FishSpec& fishSpec(int type);
int fishSaleValue(int type);

/// Fish are drawn larger than their gameplay length (stylised) so they read
/// well through the water and from the top-down camera.
constexpr float FISH_VISUAL_SCALE = 1.45f;

} // namespace lf
