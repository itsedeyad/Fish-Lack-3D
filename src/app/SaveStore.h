#pragma once
#include "game/Session.h"
#include <filesystem>
#include <string>

namespace lf {
class Game;
// Only completed progress is persistent. No pointers, fish or in-flight state.
struct ProgressData {
    Session session;
    Preferences preferences;
    MapId selectedMap=MapId::Forest;
    RoundMode mode=RoundMode::Classic;
    Difficulty difficulty=Difficulty::Normal;
    int lastMenuMap=-1;
    static ProgressData capture(const Game& game);
    void apply(Game& game) const;
    MapId advanceMenuMap();
};

class SaveStore {
public:
    explicit SaveStore(std::filesystem::path path):path_(std::move(path)){}
    bool load(ProgressData& data);
    bool save(const ProgressData& data,bool force=false);
    void poll(const ProgressData& data,float dt);
    const std::string& status() const {return status_;}
    const std::filesystem::path& path() const {return path_;}
    static std::string encode(const ProgressData& data);
    static bool decode(const std::string& text,ProgressData& data);
private:
    std::filesystem::path path_;
    std::string saved_,status_="Not saved";
    float cooldown_=0;
};
}
