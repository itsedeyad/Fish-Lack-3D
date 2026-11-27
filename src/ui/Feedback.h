#pragma once
#include "core/Settings.h"
#include "game/Game.h"
#include <string>
namespace lf::feedback {
// Pure presentation decisions shared by HUD and regression tests.
enum class TensionBand { Low, Safe, Warning, Danger };
inline TensionBand tensionBand(float value) {
    if(value < settings::tensionLow) return TensionBand::Low;
    if(value < settings::tensionWarning) return TensionBand::Safe;
    if(value < settings::tensionDanger) return TensionBand::Warning;
    return TensionBand::Danger;
}
inline const char* tensionAdvice(float value,FightPhase phase) {
    if(tensionBand(value)==TensionBand::Danger) return "RELEASE THE REEL";
    if(tensionBand(value)==TensionBand::Low) return "REEL GENTLY";
    if(tensionBand(value)==TensionBand::Warning || phase!=FightPhase::Rest) return "EASE OFF - FISH IS PULLING";
    return "HOLD TO REEL IN";
}
inline bool nibbling(const Game& g) {
    const Fish* fish=g.fishing.engagedFishPtr();
    return g.state==GameState::Waiting && fish && fish->mode==FishMode::Nibble;
}
struct EquipmentHint { std::string title,status,detail; };
inline EquipmentHint equipmentHint(const Game& g,GearCategory category,int id) {
    if(id<0 || id>=gearCount(category))return {};
    const auto& item=gearSpec(category,id);bool owned=g.session.owns(category,id);
    EquipmentHint hint;hint.title=item.name;
    hint.status=!owned?"LOCKED / "+std::to_string(item.price)+" coins":g.session.equipped(category)==id?"EQUIPPED":"OWNED";
    hint.detail=item.detail;
    if(!g.canEquip())hint.detail+=" Retrieve your line before equipping.";
    else if(!owned)hint.detail+=" Purchase from Shop, then equip in Inventory.";
    return hint;
}
inline EquipmentHint equipmentHint(const Game& g,bool rod,int id) {return equipmentHint(g,rod?GearCategory::Rod:GearCategory::Bait,id);}
}
