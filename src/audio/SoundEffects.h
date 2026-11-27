#pragma once
#include "game/Types.h"
#include <memory>
#include <string>
#include <vector>
namespace lf {
// Optional platform adapter: logic emits cues, and update drains them here.
class SoundEffects {
public:
    SoundEffects(); ~SoundEffects();
    struct Status {int loaded=0,variants=0,priority=-1,lastCue=-1,lastVariant=-1;bool device=false,busy=false;};
    Status status() const;
    void init(const std::string& directory);
    void update(const std::vector<SoundCue>& events,float volume);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
