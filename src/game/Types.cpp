#include "game/Types.h"

namespace lf {

const char* stateName(GameState s) {
    switch (s) {
    case GameState::MainMenu:     return "Main Menu";
    case GameState::Aiming:       return "Aiming";
    case GameState::Charging:     return "Charging Cast";
    case GameState::Casting:      return "Casting";
    case GameState::Waiting:      return "Waiting for a Bite";
    case GameState::BiteDetected: return "BITE!";
    case GameState::Hooking:      return "Hooking";
    case GameState::FishHooked:   return "Fish Hooked";
    case GameState::Reeling:      return "Reeling";
    case GameState::FishCaught:   return "Fish Caught";
    case GameState::FishEscaped:  return "Fish Escaped";
    case GameState::LineBroken:   return "Line Broken";
    case GameState::Paused:       return "Paused";
    case GameState::Victory:      return "Victory";
    case GameState::GameOver:     return "Game Over";
    }
    return "?";
}

bool isPlayingState(GameState s) {
    switch (s) {
    case GameState::Aiming: case GameState::Charging: case GameState::Casting:
    case GameState::Waiting: case GameState::BiteDetected: case GameState::Hooking:
    case GameState::FishHooked: case GameState::Reeling: case GameState::FishCaught:
    case GameState::FishEscaped: case GameState::LineBroken:
        return true;
    default:
        return false;
    }
}

const char* roundModeName(RoundMode mode) {
    switch(mode) {case RoundMode::Classic:return "Classic Challenge";case RoundMode::FullDay:return "Full Day";case RoundMode::Hardcore:return "Hardcore 60s";}return "";
}

const char* difficultyName(Difficulty d) {
    switch (d) {
    case Difficulty::Easy: return "Easy";
    case Difficulty::Normal: return "Normal";
    case Difficulty::Hard: return "Hard";
    }
    return "?";
}

DifficultySettings difficultySettings(Difficulty d) {
    switch (d) {
    case Difficulty::Easy:   return {4, 420.0f, 4, 0.80f, 1.40f};
    case Difficulty::Hard:   return {6, 330.0f, 2, 1.20f, 0.80f};
    case Difficulty::Normal:
    default:                 return {5, 360.0f, 3, 1.00f, 1.00f};
    }
}

static const FishSpec kSpecs[FISH_TYPE_COUNT] = {
    // name           len   speed turn  window str   stam  depth  back                          belly                         accent
    {"Bluefish",       0.62f, 2.1f, 2.4f, 1.15f, 0.35f, 6.0f, 0.65f, Color(0.10f, 0.32f, 0.70f), Color(0.80f, 0.88f, 0.95f), Color(0.05f, 0.12f, 0.35f)},
    {"Rainbow Trout",  0.85f, 1.6f, 1.9f, 0.95f, 0.50f, 9.0f, 0.85f, Color(0.33f, 0.42f, 0.22f), Color(0.92f, 0.86f, 0.78f), Color(0.92f, 0.35f, 0.45f)},
    {"Golden Carp",    1.15f, 1.1f, 1.3f, 0.80f, 0.70f, 13.0f, 1.05f, Color(0.88f, 0.55f, 0.08f), Color(1.00f, 0.88f, 0.45f), Color(0.65f, 0.30f, 0.02f)},
    {"Perch",.48f,2.4f,2.7f,1.2f,.28f,5.f,.48f,Color(.33f,.48f,.18f),Color(.91f,.84f,.56f),Color(1,.39f,.08f)},
    {"Catfish",1.10f,.9f,1.2f,1.0f,.65f,12.f,1.7f,Color(.25f,.27f,.33f),Color(.72f,.70f,.61f),Color(.12f,.15f,.19f)},
    {"Pike",1.35f,2.5f,1.8f,.85f,.85f,14.f,1.2f,Color(.23f,.39f,.25f),Color(.86f,.87f,.64f),Color(.73f,.77f,.37f)},
};

int fishSaleValue(int type) {const int prices[]={20,30,50,16,40,70};return type>=0 && type<FISH_TYPE_COUNT?prices[type]:0;}

const FishSpec& fishSpec(int type) {
    if (type < 0 || type >= FISH_TYPE_COUNT) type = 0;
    return kSpecs[type];
}

} // namespace lf
