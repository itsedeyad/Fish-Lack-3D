#include "game/Game.h"
#include "core/World.h"
#include <algorithm>

namespace lf {


Game::Game() {
    cfg = difficultySettings(difficulty);
    fishing.attach(&school, &fx, &rng);
    school.init(rng);
    fishing.reset();
    timeLeft = dayLength = cfg.dayLength;
    target = cfg.targetFish;
}

void Game::setDifficulty(Difficulty d) {
    difficulty = d;
    if (state==GameState::MainMenu) cfg = difficultySettings(d);
}

void Game::setState(GameState s) {
    state = s;
    stateTimer = 0;
}

void Game::startNewGame() {
    if(ledger.active)finishDay(DayEnd::Early);
    roundMode=selectedMode;
    int mapIndex=int(selectedMap);roundMap=mapIndex>=0 && mapIndex<3 && session.maps[mapIndex]?selectedMap:MapId::Forest;world::setMap(roundMap);
    cfg = difficultySettings(roundMode==RoundMode::Hardcore?Difficulty::Hard:difficulty);
    if(roundMode==RoundMode::FullDay){cfg.dayLength=600;cfg.targetFish=0;}
    if(roundMode==RoundMode::Hardcore){cfg.dayLength=60;cfg.targetFish=0;cfg.spareLines=1;}
    ledger=DayLedger();ledger.active=true;ledger.number=++session.daysStarted;
    ledger.order=dailyOrder(ledger.number);ledger.orderAccepted=session.nextOrderAccepted;session.nextOrderAccepted=false;
    session.baitCount[0]=std::max(5,session.baitCount[0]);selectAvailableBait();uiHistory.clear();
    summary=DaySummary();reelSoundClock=0;
    lastOutcome=Outcome::None;lastCatchQuality=100;
    caught = fishLost = perfectHooks = 0;
    for (int& c : caughtByType) c = 0;
    target = cfg.targetFish;
    linesLeft = cfg.spareLines;
    dayLength = timeLeft = cfg.dayLength;
    playTime = 0;
    lastCatchType = -1;
    lastCatchSale=0;lastCatchToxic=lastCatchSuspicious=false;
    endReason.clear();
    fx.clear();
    school.init(rng);
    fishing.reset();
    showHelp = false; overlay=Overlay::None;uiHistory.clear(); catchPending=false; awaitChargePress=false;
    applyLoadout();
    setState(GameState::Aiming);


}

void Game::setMenuMap(MapId id){
    if(ledger.active || int(id)<0 || int(id)>2)return;
    menuMap=id;
    if(state==GameState::MainMenu && world::activeMap()!=id){world::setMap(id);school.init(rng);fx.clear();fishing.reset();}
}

void Game::goToMainMenu() {
    if(ledger.active){finishDay(DayEnd::Early);return;}
    overlay=Overlay::None;uiHistory.clear(); catchPending=false;
    setState(GameState::MainMenu);
    menuIndex = 0;
    showHelp = false;
    setMenuMap(menuMap);
    fishing.reset();
}

float Game::dayProgress() const {
    if (state == GameState::MainMenu) return 0.72f; // warm title-screen light, like a late-afternoon postcard
    float elapsed=clampf(1-timeLeft/dayLength,0,1);
    return roundMode==RoundMode::FullDay?elapsed:.48f+.52f*elapsed;
}

// ================================================================== menus ==
bool Game::menuVisible() const {
    return state == GameState::MainMenu || state == GameState::Paused ||
           state == GameState::Victory || state == GameState::GameOver;
}

int Game::menuItemCount() const {
    switch (state) {
    case GameState::MainMenu: return 6;
    case GameState::Paused: return 3;
    case GameState::Victory: case GameState::GameOver: return 2;
    default: return 0;
    }
}

std::string Game::menuItemLabel(int i) const {
    switch (state) {
    case GameState::MainMenu: {
        const char* items[] = {"Play", "Inventory", "Shop", "Settings", "Credits", "Exit"};
        return items[i];
    }
    case GameState::Paused: {
        const char* items[] = {"Resume", "How to Play", "End Day"};
        return items[i];
    }
    case GameState::Victory: case GameState::GameOver: {
        const char* items[] = {"Next Day", "Main Menu"};
        return items[i];
    }
    default: return "";
    }
}

void Game::activateMenuItem(int i) {
    if(i<0 || i>=menuItemCount())return;
    emitSound(SoundCue::Click);
    switch (state) {
    case GameState::MainMenu:
        if (i == 0) openOverlay(Overlay::TripSetup);
        else if (i == 1) openOverlay(Overlay::Inventory);
        else if (i == 2) openOverlay(Overlay::Shop);
        else if (i == 3) openOverlay(Overlay::Settings);
        else if (i == 4) openOverlay(Overlay::Credits);
        else if (i == 5) quitRequested = true;
        break;
    case GameState::Paused:
        if (i == 0) state = pausedFrom; // resume exactly where we left off
        else if (i == 1) openHelp();
        else if (i == 2) finishDay(DayEnd::Early);
        break;
    case GameState::Victory: case GameState::GameOver:
        if (i == 0) openOverlay(Overlay::TripSetup);
        else goToMainMenu();
        break;
    default: break;
    }
}

// ================================================================= update ==
void Game::update(float dt, InputState& in) {
    uiTime+=dt;purchasePulse=std::max(0.f,purchasePulse-dt);
    if(overlay==Overlay::NoBait || overlay==Overlay::Secret){if(in.wasPressed(KEY_ESC) || in.wasPressed(KEY_ENTER))closeOverlay();return;}
    if(overlay==Overlay::DailyOrder){if(in.wasPressed(KEY_ESC))closeOverlay();else if(in.wasPressed(KEY_ENTER))confirmStart(true);return;}
    if(showHelp) {if(in.wasPressed('h') || in.wasPressed(KEY_ESC) || in.wasPressed(KEY_ENTER))closeOverlay();return;}
    if(in.wasPressed('h')){openHelp();return;}
    if (overlay!=Overlay::None) {
        updateOverlay(in);
        return;
    }
    if(!showHelp) {
        if(in.wasPressed('i')) { openOverlay(Overlay::Inventory); return; }
        if(in.wasPressed('u')) { openOverlay(Overlay::Shop); return; }
        if(in.wasPressed('o')) { openOverlay(Overlay::Settings); return; }
        if(in.wasPressed('b')) cycleBait();
    }
    if(catchPending && state==GameState::FishCaught) {
        updateCatchChoice(in);
        return;
    }


    if (state == GameState::Paused) {
        // Nothing moves while paused, only the menu is active.
        if (in.wasPressed('p') || in.wasPressed(KEY_ESC)) { state = pausedFrom; return; }
    }

    if (menuVisible()) {
        updateMenu(dt,in);
        return;
    }

    worldTime += dt;
    updatePlaying(dt, in);
    if (state == GameState::Paused || showHelp) return;
    school.update(dt, worldTime, rng, caught);
    fx.update(dt);
}

void Game::updateTripInput(InputState& in) {
    if(in.wasPressed('q') || in.wasPressed('e')){
        int direction=in.wasPressed('q')?-1:1;
        for(int step=1;step<=3;++step){int id=(int(selectedMap)+direction*step+6)%3;if(session.maps[id]){selectMap(MapId(id));break;}}
    }
    if(in.wasPressed('i')){openOverlay(Overlay::Inventory);return;}
    if(in.wasPressed('u')){openOverlay(Overlay::Shop);return;}
    if(in.wasPressed(KEY_UP) || in.wasPressed('w'))selectedMode=RoundMode((int(selectedMode)+2)%3);
    if(in.wasPressed(KEY_DOWN) || in.wasPressed('s'))selectedMode=RoundMode((int(selectedMode)+1)%3);
    if(selectedMode!=RoundMode::Hardcore) {
        if(in.wasPressed(KEY_LEFT) || in.wasPressed('a'))setDifficulty(Difficulty((int(difficulty)+2)%3));
        if(in.wasPressed(KEY_RIGHT) || in.wasPressed('d'))setDifficulty(Difficulty((int(difficulty)+1)%3));
    }
    if(in.wasPressed(KEY_ENTER)){requestStart();in.releaseAll();}
    return;
}


void Game::updateOverlay(InputState& in) {
    if(in.wasPressed(KEY_ESC)){closeOverlay();in.releaseAll();return;}
    if(overlay==Overlay::Credits){if(in.wasPressed(KEY_ENTER))closeOverlay();return;}
    if(overlay==Overlay::TripSetup) {
        updateTripInput(in);
        return;
    }
    if(in.wasPressed('i') || in.wasPressed('u') || in.wasPressed('o')) {
        Overlay requested=Overlay::Settings;
        if(in.wasPressed('i'))requested=Overlay::Inventory;
        else if(in.wasPressed('u'))requested=Overlay::Shop;
        if(requested==overlay)closeOverlay();
        else openOverlay(requested);
        in.releaseAll();
        return;
    }
    if(overlay==Overlay::Settings) {
        if(in.wasPressed(KEY_DOWN) || in.wasPressed('s'))overlayIndex=(overlayIndex+1)%5;
        if(in.wasPressed(KEY_UP) || in.wasPressed('w'))overlayIndex=(overlayIndex+4)%5;
        if(in.wasPressed(KEY_LEFT) || in.wasPressed('a'))adjustSetting(overlayIndex,-1);
        if(in.wasPressed(KEY_RIGHT) || in.wasPressed('d'))adjustSetting(overlayIndex,1);
        if(in.wasPressed(KEY_ENTER))adjustSetting(overlayIndex,1);
    } else {
        int tab=locationsTab?5:int(gearCategory),tabs=overlay==Overlay::Shop?6:5;
        if(in.wasPressed(KEY_LEFT) || in.wasPressed('a'))selectCategory((tab+tabs-1)%tabs);
        if(in.wasPressed(KEY_RIGHT) || in.wasPressed('d'))selectCategory((tab+1)%tabs);
        if(overlay==Overlay::Shop && gearCategory==GearCategory::Bait){
            if(in.wasPressed('+') || in.wasPressed('='))adjustPurchaseQuantity(1);
            if(in.wasPressed('-'))adjustPurchaseQuantity(-1);
        }
        auto items=visibleItems();int count=int(items.size());
        if(locationsTab && in.wasPressed(KEY_ENTER))buyMap(MapId(overlayIndex%3));
        if(locationsTab){if(in.wasPressed(KEY_DOWN))overlayIndex=(overlayIndex+1)%3;if(in.wasPressed(KEY_UP))overlayIndex=(overlayIndex+2)%3;}
        if(count) {
            if(in.wasPressed(KEY_DOWN) || in.wasPressed('s'))overlayIndex=(overlayIndex+1)%count;
            if(in.wasPressed(KEY_UP) || in.wasPressed('w'))overlayIndex=(overlayIndex+count-1)%count;
            if(in.wasPressed(KEY_ENTER))activateOverlay(items[overlayIndex]);
        }
    }
}

void Game::updateCatchChoice(InputState& in) {
    if(in.wasPressed(KEY_LEFT) || in.wasPressed(KEY_RIGHT)) menuIndex=1-menuIndex;
    if(in.wasPressed(KEY_ENTER)) chooseCatch(menuIndex==0);
    else if(in.wasPressed('k')) chooseCatch(true);
    else if(in.wasPressed('l')) chooseCatch(false);
    else if(in.wasPressed('p') || in.wasPressed(KEY_ESC)) { pausedFrom=state; state=GameState::Paused; menuIndex=0; }
}

void Game::updateMenu(float dt,InputState& in) {
    int n = menuItemCount();
    if (in.wasPressed('w') || in.wasPressed(KEY_UP)) menuIndex = (menuIndex + n - 1) % n;
    if (in.wasPressed('s') || in.wasPressed(KEY_DOWN)) menuIndex = (menuIndex + 1) % n;

    if (state == GameState::MainMenu && in.wasPressed(KEY_ESC)) quitRequested = true;
    if (in.wasPressed(KEY_ENTER) || in.wasPressed(KEY_SPACE)) {
        in.consume(KEY_SPACE);
        activateMenuItem(menuIndex);
        return;
    }
    if (state == GameState::Paused) return;
    // Menu / end screens: the lake stays alive in the background.
    stateTimer += dt;
    worldTime += dt;
    school.update(dt, worldTime, rng, caught);
    fx.update(dt);
    fishing.updateAnimation(dt, worldTime, state == GameState::MainMenu ? GameState::Aiming : GameState::FishEscaped);
}

void Game::handleOutcome(Outcome o) {
    if(o!=Outcome::None)lastOutcome=o;
    switch (o) {
    case Outcome::None:
    case Outcome::Landed:
        break;
    case Outcome::NothingBiting:

        break;
    case Outcome::Spooked:
        ++fishLost;

        setState(GameState::FishEscaped);
        break;
    case Outcome::Bite:
        emitSound(SoundCue::Bite);

        setState(GameState::BiteDetected);
        break;
    case Outcome::BiteMissed:
        ++fishLost;

        setState(GameState::FishEscaped);
        break;
    case Outcome::Hooked:
        fishing.beginHook();
        if (fishing.lastHookPerfect()) {
            ++perfectHooks;


        }
        setState(GameState::Hooking);
        break;
    case Outcome::Retrieved:

        fishing.resetForNextCast();selectAvailableBait();
        setState(GameState::Aiming);
        break;
    case Outcome::Caught: {
        const Fish* f = fishing.engagedFishPtr();
        int type = f ? f->type : 0;

        ++caught;
        ++caughtByType[type];
        lastCatchType = type;
        lastCatchQuality=fishing.fishQuality();lastCatchSale=fishSaleValue(type)*lastCatchQuality/100;
        lastCatchToxic=f && f->toxic;lastCatchSuspicious=f && f->suspicious;

        emitSound(SoundCue::Catch);
        fishing.beginLanding();
        catchPending=true; menuIndex=0; setState(GameState::FishCaught);
        break;
    }
    case Outcome::EscapedSlack:
        ++fishLost;
        fishing.loseFish();

        setState(GameState::FishEscaped);
        break;
    case Outcome::EscapedRun:
        ++fishLost;
        fishing.loseFish();

        setState(GameState::FishEscaped);
        break;
    case Outcome::LineSnapped:
        ++fishLost;
        --linesLeft;
        fishing.loseFish();
        emitSound(SoundCue::LineBreak);
        fishing.snapLine();

        setState(GameState::LineBroken);
        break;
    }
}

// Aim with the keyboard (A/D/W/S) or, if the mouse moved last, with the
// cursor: horizontal position = direction, vertical position = cast angle.
void Game::aim(float dt, const FishingControls& c, InputState& in) {
    if (c.left || c.right || c.up || c.down) in.mouseAim = false;
    if (in.mouseAim) fishing.aimTowards(-in.mouseX * deg2rad(30), deg2rad(42.5f - in.mouseY * 20.0f), dt * preferences.sensitivity);
    else fishing.updateAim(dt, c);
}

void Game::updatePlaying(float dt, InputState& in) {
    if (in.wasPressed('p') || in.wasPressed(KEY_ESC)) {
        suspendControls();
        pausedFrom = state;
        state = GameState::Paused;
        menuIndex = 0;
        return;
    }

#ifdef LF_DEVELOPMENT
    if (in.wasPressed(KEY_F5)) { // developer shortcut: jump towards sunset
        timeLeft = std::fmax(5.0f, timeLeft - 60.0f);

    }

#endif

    // Modal windows return earlier in update(), before the round clock advances.
    stateTimer += dt;
    playTime += dt;
    if (state != GameState::FishCaught) {
        timeLeft -= dt;
        if (timeLeft <= 0) {
            timeLeft = 0;
            fishing.loseFish();
            finishDay(DayEnd::Sunset);
            return;
        }
    }

    FishingControls c;
    c.left = in.isHeld('a') || in.isHeld(KEY_LEFT);
    c.right = in.isHeld('d') || in.isHeld(KEY_RIGHT);
    c.up = in.isHeld('w') || in.isHeld(KEY_UP);
    c.down = in.isHeld('s') || in.isHeld(KEY_DOWN);
    c.hookPressed = in.wasPressed('e');
    c.reelHeld = in.isHeld('r');
    if(state==GameState::Waiting || state==GameState::BiteDetected || state==GameState::Hooking || state==GameState::FishHooked || state==GameState::Reeling) {
        if(c.left || c.right || c.up || c.down)in.mouseAim=false;
        float yaw=in.mouseAim?-in.mouseX*deg2rad(12):(float(c.left)-float(c.right))*deg2rad(12);
        float pitch=in.mouseAim?-in.mouseY*deg2rad(15):(float(c.up)-float(c.down))*deg2rad(15);
        fishing.guideRod(yaw,pitch,dt);
    }
    reelSoundClock-=dt;
    if(c.reelHeld && (state==GameState::Reeling || state==GameState::Waiting) && reelSoundClock<=0) {
        emitSound(SoundCue::Reel);reelSoundClock=.12f;
    }


    switch (state) {
    case GameState::Aiming:
        aim(dt, c, in);
        if (in.wasPressed(KEY_SPACE)) {
            if(!hasBait()){break;}
            fishing.beginCharge();
            setState(GameState::Charging);
        }
        break;
    case GameState::Charging:
        if(awaitChargePress) {
            if(!in.wasPressed(KEY_SPACE)) break;
            awaitChargePress=false;
        }
        aim(dt, c, in);
        fishing.updateCharge(dt);
        if (!in.isHeld(KEY_SPACE)) {
            emitSound(SoundCue::Cast);
            fishing.beginCast(); setState(GameState::Casting);
        }
        break;
    case GameState::Casting:
        {bool released=fishing.castReleased();Outcome cast=fishing.updateCast(dt);
        if(!released && fishing.castReleased()) {if(session.baitCount[session.bait]>0)--session.baitCount[session.bait];++ledger.baitUsed;}
        if (cast == Outcome::Landed) {
            emitSound(SoundCue::Splash);
            setState(GameState::Waiting);

        }
        }break;
    case GameState::Waiting:
        handleOutcome(fishing.updateWaiting(dt, c, worldTime));
        break;
    case GameState::BiteDetected:
        handleOutcome(fishing.updateBite(dt, c));
        break;
    case GameState::Hooking:
        fishing.updateHookedPreview(dt, worldTime);
        if (stateTimer > 0.45f) {
            setState(GameState::FishHooked);

        }
        break;
    case GameState::FishHooked:
        fishing.updateHookedPreview(dt, worldTime);
        if (stateTimer > 1.0f) {
            constexpr int maxDifficultyIncreases=6;
            constexpr float strengthIncreasePerCatch=.07f;
            fishing.beginFight(1.0f + strengthIncreasePerCatch * std::min(caught,maxDifficultyIncreases));
            setState(GameState::Reeling);
        }
        break;
    case GameState::Reeling:
        handleOutcome(fishing.updateFight(dt, c, worldTime, cfg.tensionMul * session.stats().tension));
        break;
    case GameState::FishCaught:
        break; // waiting for an explicit Keep / Release choice
    case GameState::FishEscaped:
        fishing.updateResult(dt);
        if (stateTimer > 2.2f) {
            fishing.resetForNextCast();selectAvailableBait();
            setState(GameState::Aiming);
        }
        break;
    case GameState::LineBroken:
        fishing.updateResult(dt);
        if (stateTimer > 2.4f) {
            if (linesLeft <= 0) {
                finishDay(DayEnd::NoLines);
                return;
            }
            fishing.resetForNextCast();selectAvailableBait();
            setState(GameState::Aiming);
        }
        break;
    default:
        break;
    }
    checkSupplies();
    if (isPlayingState(state)) fishing.updateAnimation(dt, worldTime, state);
}


void Game::applyLoadout() {
    auto stats=session.stats();fishing.setLoadout(stats);fishing.setBiteWindowMul(cfg.biteWindowMul*stats.bite);
}
bool Game::equip(GearCategory category,int id) {
    if(!canEquip() || !session.owns(category,id)){emitSound(SoundCue::Reject);return false;}
    session.equipped(category)=id;applyLoadout();emitSound(SoundCue::Equip);return true;
}
bool Game::buy(GearCategory category,int id,int quantity) {
    if(!shopAvailable() || !session.purchase(category,id,quantity)){emitSound(SoundCue::Reject);return false;}
    purchasePulse=.5f;

    emitSound(SoundCue::Purchase);return true;
}
bool Game::equip(bool rod,int id){return equip(rod?GearCategory::Rod:GearCategory::Bait,id);}
bool Game::buy(bool rod,int id){return buy(rod?GearCategory::Rod:GearCategory::Bait,id);}
void Game::cycleBait() {
    if(!canEquip()){return;}
    for(int n=1;n<=gearCount(GearCategory::Bait);++n) {
        int id=(session.bait+n)%gearCount(GearCategory::Bait);
        if(session.owns(GearCategory::Bait,id)){equip(GearCategory::Bait,id);return;}
    }
}
void Game::chooseCatch(bool keep) {
    if(!catchPending || state!=GameState::FishCaught)return;
    bool wasComplete=ledger.orderComplete();
    if(keep && lastCatchType>=0)ledger.keep(lastCatchType,lastCatchQuality,1,lastCatchToxic);
    else if(lastCatchType>=0)++ledger.released[lastCatchType];
    if(keep && lastCatchToxic)emitSound(SoundCue::Poison);else if(!keep)emitSound(SoundCue::Release);
    if(!wasComplete && ledger.orderComplete())emitSound(SoundCue::Complete);
    catchPending=false;
    if(keep)fishing.finishCatch();else fishing.releaseCatch();
    if(roundMode==RoundMode::Classic && caught>=target)finishDay(DayEnd::Goal);
    else {fishing.resetForNextCast();selectAvailableBait();setState(GameState::Aiming);checkSupplies();}
}
void Game::finishDay(DayEnd reason) {
    // Every exit path reaches this guard; a completed day can never pay twice.
    if(!ledger.active)return;
    if(catchPending) {
        if(lastCatchType>=0)++ledger.released[lastCatchType];
        fishing.releaseCatch();
        catchPending=false;
    }
    fishing.loseFish();

    // Freeze the receipt before clearing the live catch bag.
    summary=DaySummary();
    summary.valid=true;
    summary.number=ledger.number;
    summary.mode=roundMode;
    summary.reason=reason;
    for(int type=0;type<FISH_TYPE_COUNT;++type) {
        summary.caught[type]=caughtByType[type];
        summary.released[type]=ledger.released[type];
        summary.toxic[type]=ledger.toxicCount(type);
        summary.sold[type]=ledger.safeCount(type);
        summary.saleTotals[type]=ledger.saleTotal(type);
        summary.qualityAverage[type]=ledger.averageQuality(type);
    }
    summary.map=roundMap;
    summary.gross=ledger.pendingValue();
    summary.spent=ledger.spent;
    summary.order={ledger.orderAccepted,ledger.order.type,ledger.order.quantity,ledger.safeCount(ledger.order.type)};
    summary.orderBonus=ledger.orderComplete()?ledger.order.reward:0;
    summary.totalIncome=summary.gross+summary.orderBonus;
    summary.cleaningFee=std::min(ledger.cleaningCost(),session.coins+summary.totalIncome);
    summary.net=summary.totalIncome-summary.cleaningFee;
    session.coins+=summary.net;
    summary.closingCoins=session.coins;

    ledger.active=false;
    ledger.kept.clear();
    overlay=Overlay::None;
    uiHistory.clear();
    showHelp=false;
    switch(reason) {
    case DayEnd::Goal:endReason="Catch goal completed.";break;
    case DayEnd::Sunset:endReason="The sun has set over the lake.";break;
    case DayEnd::NoLines:endReason="You ran out of fishing line.";break;
    case DayEnd::NoBait:endReason="All bait supplies have been used.";break;
    default:endReason="You finished the day early.";break;
    }
    const bool completed=reason==DayEnd::Goal || (reason==DayEnd::Sunset && roundMode!=RoundMode::Classic);
    setState(completed?GameState::Victory:GameState::GameOver);
    menuIndex=0;
    emitSound(reason==DayEnd::NoBait?SoundCue::NoBait:summary.gross?SoundCue::Coins:SoundCue::DayEnd);
    if(reason==DayEnd::NoBait)overlay=Overlay::NoBait;
}
bool Game::selectMap(MapId id) {
    int index=int(id);if(ledger.active || index<0 || index>=3 || !session.maps[index])return false;
    selectedMap=id;return true;
}
bool Game::buyMap(MapId id) {
    int index=int(id);if(!shopAvailable() || index<0 || index>=3 || session.maps[index])return false;
    int price=world::mapSpec(id).price;if(session.coins<price)return false;
    session.coins-=price;session.maps[index]=true;purchasePulse=.5f;emitSound(SoundCue::Unlock);return true;
}

bool Game::hasBait() const {for(int count:session.baitCount)if(count>0)return true;return false;}
void Game::selectAvailableBait() {
    if(session.baitCount[session.bait]<=0)for(int id=0;id<gearCount(GearCategory::Bait);++id)if(session.baitCount[id]>0){session.bait=id;break;}
    applyLoadout();
}
void Game::acceptOrder() {if(!shopAvailable())return;session.nextOrderAccepted=true;emitSound(SoundCue::Accept);}
void Game::openHelp() {
    suspendControls();uiHistory.push_back({overlay,gearCategory,overlayIndex,locationsTab});showHelp=true;
}
void Game::openOverlay(Overlay value) {
    if(value==Overlay::Shop && !shopAvailable()){return;}
    if(overlay==value)return;
    suspendControls();
    bool peers=(overlay==Overlay::Shop || overlay==Overlay::Inventory) && (value==Overlay::Shop || value==Overlay::Inventory);
    if(!peers)uiHistory.push_back({overlay,gearCategory,overlayIndex,locationsTab});
    if(value==Overlay::Credits)secretClicks=0;
    if(value==Overlay::Shop)purchaseQuantity=1;
    emitSound(SoundCue::Open);
    overlay=value;overlayIndex=0;locationsTab=false;
}
void Game::closeOverlay() {
    if(overlay==Overlay::Credits)secretClicks=0;
    emitSound(SoundCue::Close);
    showHelp=false;
    if(uiHistory.empty()){overlay=Overlay::None;overlayIndex=0;}
    else {auto frame=uiHistory.back();uiHistory.pop_back();overlay=frame.overlay;gearCategory=frame.category;overlayIndex=frame.index;locationsTab=frame.locations;}
}
void Game::selectCategory(int category) {
    int tabs=overlay==Overlay::Shop?6:5;if(category<0 || category>=tabs)return;
    locationsTab=category==5 && overlay==Overlay::Shop;if(!locationsTab)gearCategory=GearCategory(category);overlayIndex=0;emitSound(SoundCue::Click);
}
std::vector<int> Game::visibleItems() const {
    std::vector<int> items;if(locationsTab)return items;
    for(int i=0;i<gearCount(gearCategory);++i)if(overlay==Overlay::Shop || session.owns(gearCategory,i))items.push_back(i);
    return items;
}
void Game::activateOverlay(int action) {
    if(action==99){closeOverlay();return;}
    if(overlay==Overlay::Settings){adjustSetting(action,1);return;}
    if(overlay==Overlay::Shop)buy(gearCategory,action,purchaseQuantity);
    else if(overlay==Overlay::Inventory)equip(gearCategory,action);
}
void Game::adjustPurchaseQuantity(int delta){purchaseQuantity=std::clamp(purchaseQuantity+delta,1,99);}
void Game::adjustSetting(int row,int dir) {
    switch(row) {
    case 0:preferences.sensitivity=clampf(preferences.sensitivity+.1f*dir,.3f,2);break;
    case 1:preferences.brightness=clampf(preferences.brightness+.05f*dir,.65f,1.25f);break;
    case 2:preferences.sfx=clampf(preferences.sfx+.1f*dir,0,1);break;
    case 3:preferences.camera=(preferences.camera+4+dir)%4;break;
    case 4:preferences.fullscreen=!preferences.fullscreen;break;
    default:return;
    }
    emitSound(SoundCue::Click);
}
// Only actual starts advance the day or replenish Worm supplies.
void Game::requestStart(){if(!ledger.active && overlay==Overlay::TripSetup)openOverlay(Overlay::DailyOrder);}
void Game::confirmStart(bool accept){if(overlay!=Overlay::DailyOrder || ledger.active)return;session.nextOrderAccepted=accept;if(accept)emitSound(SoundCue::Accept);startNewGame();}
void Game::checkSupplies(){if(ledger.active && state==GameState::Aiming && !hasBait())finishDay(DayEnd::NoBait);}
void Game::pause(){if(!isPlaying())return;suspendControls();pausedFrom=state;state=GameState::Paused;menuIndex=0;}
void Game::clickCreditsFish(){
    if(overlay!=Overlay::Credits || session.secretClaimed)return;
    if(++secretClicks==7){session.secretClaimed=true;session.coins=std::max(session.coins,1000000);openOverlay(Overlay::Secret);emitSound(SoundCue::Secret);}
}
void Game::emitSound(SoundCue cue) {
    if(soundEvents.size()>=24)soundEvents.erase(soundEvents.begin());
    soundEvents.push_back(cue);
}
std::vector<SoundCue> Game::takeSounds() {auto events=soundEvents;soundEvents.clear();return events;}
} // namespace lf
