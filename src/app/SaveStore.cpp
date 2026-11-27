#include "app/SaveStore.h"
#include "game/Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace lf {
namespace {
bool valid(const ProgressData& d){
    if(d.session.coins<0 || d.session.coins>1000000000 || d.session.daysStarted<0 || d.session.daysStarted>1000000)return false;
    if(int(d.selectedMap)<0 || int(d.selectedMap)>2 || !d.session.maps[0] || !d.session.maps[int(d.selectedMap)])return false;
    if(int(d.mode)<0 || int(d.mode)>2 || int(d.difficulty)<0 || int(d.difficulty)>2 || d.lastMenuMap< -1 || d.lastMenuMap>2)return false;
    auto range=[](float x,float lo,float hi){return std::isfinite(x) && x>=lo && x<=hi;};
    const auto& p=d.preferences;
    if(!range(p.sensitivity,.3f,2) || !range(p.brightness,.65f,1.25f) || !range(p.sfx,0,1) || p.camera<0 || p.camera>3)return false;
    for(int count:d.session.baitCount)if(count<0 || count>1000000)return false;
    for(int c=0;c<GEAR_CATEGORIES;++c){auto category=GearCategory(c);int id=d.session.equipped(category);
        if(id<0 || id>=gearCount(category))return false;
        // The last piece can be in flight when the profile is checkpointed.
        if(category!=GearCategory::Bait && (!d.session.owns(category,0) || !d.session.owns(category,id)))return false;
    }
    return true;
}
bool tag(std::istream& in,const char* expected){std::string value;return bool(in>>value) && value==expected;}
bool flag(std::istream& in,bool& out){int n=-1;if(!(in>>n) || n<0 || n>1)return false;out=n!=0;return true;}
}
ProgressData ProgressData::capture(const Game& g){
    ProgressData d;d.session=g.session;d.session.nextOrderAccepted=false;d.preferences=g.preferences;
    d.selectedMap=g.selectedMap;d.mode=g.selectedMode;d.difficulty=g.difficulty;d.lastMenuMap=int(g.menuMap);return d;
}
void ProgressData::apply(Game& g) const {
    g.session=session;g.session.nextOrderAccepted=false;g.preferences=preferences;g.selectedMap=selectedMap;
    g.selectedMode=mode;g.setDifficulty(difficulty);g.selectAvailableBait();
}
MapId ProgressData::advanceMenuMap(){lastMenuMap=(lastMenuMap+1)%3;return MapId(lastMenuMap);}
std::string SaveStore::encode(const ProgressData& d){
    std::ostringstream out;out<<std::setprecision(9)<<"FISH_LACK_SAVE 1\n";
    out<<"wallet "<<d.session.coins<<"\ndays "<<d.session.daysStarted<<"\nsecret "<<d.session.secretClaimed<<"\nbait";
    for(int count:d.session.baitCount)out<<' '<<count;
    for(auto c:{GearCategory::Rod,GearCategory::Reel,GearCategory::Line,GearCategory::Float}){out<<"\nowned_"<<int(c);for(int id=0;id<gearCount(c);++id)out<<' '<<d.session.owns(c,id);}
    out<<"\nequipped";for(int c=0;c<GEAR_CATEGORIES;++c)out<<' '<<d.session.equipped(GearCategory(c));
    out<<"\nmaps";for(bool owned:d.session.maps)out<<' '<<owned;
    out<<"\ntrip "<<int(d.selectedMap)<<' '<<int(d.mode)<<' '<<int(d.difficulty);
    out<<"\npreferences "<<d.preferences.sensitivity<<' '<<d.preferences.brightness<<' '<<d.preferences.sfx<<' '<<d.preferences.camera<<' '<<d.preferences.fullscreen;
    out<<"\nmenu "<<d.lastMenuMap<<"\nEND\n";return out.str();
}
bool SaveStore::decode(const std::string& text,ProgressData& data){
    if(text.size()>8192)return false;
    ProgressData d;std::istringstream in(text);int version=0,map=0,mode=0,difficulty=0;
    if(!tag(in,"FISH_LACK_SAVE") || !(in>>version) || version!=1)return false;
    if(!tag(in,"wallet") || !(in>>d.session.coins) || !tag(in,"days") || !(in>>d.session.daysStarted) || !tag(in,"secret") || !flag(in,d.session.secretClaimed) || !tag(in,"bait"))return false;
    for(int& count:d.session.baitCount)if(!(in>>count))return false;
    for(auto c:{GearCategory::Rod,GearCategory::Reel,GearCategory::Line,GearCategory::Float}){
        std::string key="owned_"+std::to_string(int(c));if(!tag(in,key.c_str()))return false;
        for(int id=0;id<gearCount(c);++id)if(!flag(in,d.session.ownership(c)[id]))return false;
    }
    if(!tag(in,"equipped"))return false;
    for(int c=0;c<GEAR_CATEGORIES;++c)if(!(in>>d.session.equipped(GearCategory(c))))return false;
    if(!tag(in,"maps"))return false;
    for(bool& owned:d.session.maps)if(!flag(in,owned))return false;
    if(!tag(in,"trip") || !(in>>map>>mode>>difficulty))return false;
    if(map<0 || map>2 || mode<0 || mode>2 || difficulty<0 || difficulty>2)return false;
    d.selectedMap=MapId(map);d.mode=RoundMode(mode);d.difficulty=Difficulty(difficulty);
    if(!tag(in,"preferences") || !(in>>d.preferences.sensitivity>>d.preferences.brightness>>d.preferences.sfx>>d.preferences.camera) || !flag(in,d.preferences.fullscreen))return false;
    if(!tag(in,"menu") || !(in>>d.lastMenuMap) || !tag(in,"END"))return false;
    in>>std::ws;if(!in.eof() || !valid(d))return false;
    data=d;return true; // A partial/corrupt file never partially mutates progress.
}
bool SaveStore::load(ProgressData& data) {
    std::error_code error;
    const auto size=std::filesystem::file_size(path_,error);
    if(error) {
        status_=std::filesystem::exists(path_,error)?"Saving unavailable":"New profile";
        return false;
    }
    if(size>8192) {
        status_="Invalid save; defaults loaded";
        return false;
    }
    std::ifstream input(path_,std::ios::binary);
    std::string text(size,'\0');
    if(!input.read(text.data(),std::streamsize(size))) {
        status_="Saving unavailable";
        return false;
    }
    if(!decode(text,data)) {
        status_="Invalid save; defaults loaded";
        return false;
    }
    saved_=text;
    status_="Saved";
    return true;
}

bool SaveStore::save(const ProgressData& data,bool force) {
    if(!valid(data)) {
        status_="Saving unavailable";
        return false;
    }
    const std::string text=encode(data);
    if(!force && text==saved_)return true;

    // Write beside the destination so rename can replace it on the same volume.
    auto temporary=path_;
    temporary+=".tmp";
    {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        output.write(text.data(),std::streamsize(text.size()));
        output.flush();
        if(!output) {
            status_="Saving unavailable";
            return false;
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary,path_,error);
    if(error) {
        std::filesystem::remove(temporary,error);
        status_="Saving unavailable";
        return false;
    }
    saved_=text;
    status_="Saved";
    return true;
}

void SaveStore::poll(const ProgressData& data,float dt) {
    constexpr float autosaveInterval=.5f;
    cooldown_=std::max(0.f,cooldown_-dt);
    if(cooldown_>0)return;
    save(data);
    cooldown_=autosaveInterval; // Avoid disk writes for every slider movement.
}
} // namespace lf
