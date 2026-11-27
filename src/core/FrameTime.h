#pragma once
#include "core/Settings.h"
#include <algorithm>

namespace lf {
// Keep all elapsed time, but never pass a large step to the simulation.
template<class Update>
void advanceFrame(float elapsed, Update update) {
    while (elapsed > 0.0f) {
        float dt = std::min(elapsed, settings::maxStep);
        update(dt);
        elapsed -= dt;
    }
}
} // namespace lf
