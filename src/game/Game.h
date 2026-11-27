// =============================================================================
//  Game.h - top-level game logic: the state machine, day timer, menus.
//
//  Game knows nothing about OpenGL. Each frame the application calls
//  update(dt, input); the renderer and HUD then read the public state.
//  This separation is what allows tests/test_gameplay.cpp to play complete
//  games without opening a window.
// =============================================================================
#pragma once
#include "core/MathUtil.h"
#include "game/Types.h"
#include "game/Input.h"
#include "game/Fish.h"
#include "game/Effects.h"
#include "game/FishingSystem.h"
#include "game/Session.h"
#include <string>

namespace lf {

class Game {
public:
    Game();
    Game(const Game&)=delete;
    Game& operator=(const Game&)=delete;
    Game(Game&&)=delete;
    Game& operator=(Game&&)=delete;

    /// Advance the simulation by dt seconds using the current input.
    void update(float dt, InputState& in);

    void startNewGame();          // reset the round, preserve session progress
    void goToMainMenu();
    void setDifficulty(Difficulty d);

    Session session;
    MapId selectedMap=MapId::Forest,roundMap=MapId::Forest,menuMap=MapId::Forest;
    void setMenuMap(MapId id);
    std::string saveStatus;
    bool selectMap(MapId id);
    bool buyMap(MapId id);
    Outcome lastOutcome=Outcome::None;
    int lastCatchQuality=100,lastCatchSale=0;
    bool lastCatchToxic=false,lastCatchSuspicious=false;
    int secretClicks=0;
    void clickCreditsFish();
    void requestStart();
    void confirmStart(bool accept);
    void pause();
    void checkSupplies();
    Preferences preferences;
    Overlay overlay=Overlay::None;
    RoundMode selectedMode=RoundMode::Classic,roundMode=RoundMode::Classic;
    GearCategory gearCategory=GearCategory::Rod;
    bool locationsTab=false;
    DayLedger ledger;
    DaySummary summary;
    void finishDay(DayEnd reason);
    void closeOverlay();
    void selectCategory(int category);
    std::vector<int> visibleItems() const;
    void applyLoadout();
    bool buy(GearCategory category,int id,int quantity=1);
    bool shopAvailable() const {return !ledger.active;}
    bool hasBait() const;
    void selectAvailableBait();
    void acceptOrder();
    void openHelp();
    float uiTime=0,purchasePulse=0;
    int purchaseQuantity=1;
    void adjustPurchaseQuantity(int delta);
    struct UiFrame {Overlay overlay;GearCategory category;int index;bool locations=false;};
    std::vector<UiFrame> uiHistory;
    bool equip(GearCategory category,int id);
    void emitSound(SoundCue cue);
    std::vector<SoundCue> takeSounds();
    std::vector<SoundCue> soundEvents;
    float reelSoundClock=0;

    int overlayIndex=0;
    bool catchPending=false;
    bool awaitChargePress=false;
    void suspendControls() { if(activeState()==GameState::Charging) awaitChargePress=true; }
    bool uiBlocked() const { return menuVisible() || showHelp || overlay!=Overlay::None || catchPending; }
    bool canEquip() const { return state==GameState::MainMenu || state==GameState::Aiming || state==GameState::Victory || state==GameState::GameOver || (state==GameState::Paused && pausedFrom==GameState::Aiming); }
    bool equip(bool rod,int id);
    bool buy(bool rod,int id);
    void cycleBait();
    void chooseCatch(bool keep);
    void openOverlay(Overlay value);
    void activateOverlay(int action);
    void adjustSetting(int row,int direction);

    // ---- menus (shared by keyboard, mouse and HUD drawing) ----
    int  menuItemCount() const;
    std::string menuItemLabel(int i) const;
    void activateMenuItem(int i);
    bool menuVisible() const;

    // ---- derived values used by the renderer / HUD ----
    /// 0 at the start of the day, 1 at sunset.
    float dayProgress() const;
    bool  isPlaying() const { return isPlayingState(state); }
    /// State used for gameplay animation (the state we paused from, if paused).
    GameState activeState() const { return state == GameState::Paused ? pausedFrom : state; }

    // ---- public state ----
    GameState  state = GameState::MainMenu;
    GameState  pausedFrom = GameState::Aiming;
    Difficulty difficulty = Difficulty::Normal;
    DifficultySettings cfg;

    int   caught = 0, target = 5, linesLeft = 3;
    int   caughtByType[FISH_TYPE_COUNT] = {};
    int   fishLost = 0, perfectHooks = 0;
    float timeLeft = 0, dayLength = 360;
    float stateTimer = 0;     // seconds spent in the current state
    float worldTime = 0;      // always running (water, fish animation)
    float playTime = 0;

    int   menuIndex = 0;
    bool  showHelp = false;
    bool  quitRequested = false;

    std::string endReason;
    int   lastCatchType = -1;

    FishingSystem fishing;
    FishSchool    school;
    Effects       fx;
    Rng           rng{20261124u};

private:
    void setState(GameState s);
    void updateTripInput(InputState& in);
    void updateOverlay(InputState& in);
    void updateCatchChoice(InputState& in);
    void updateMenu(float dt,InputState& in);
    void updatePlaying(float dt, InputState& in);
    void aim(float dt, const FishingControls& c, InputState& in);
    void handleOutcome(Outcome o);
};

} // namespace lf
