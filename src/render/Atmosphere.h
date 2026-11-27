// =============================================================================
//  Atmosphere.h - time-of-day colours. dayProgress 0 = early afternoon,
//  1 = sunset. Colours are interpolated between hand-picked key frames, and
//  the sun direction sinks towards the horizon over the lake.
// =============================================================================
#pragma once
#include "core/MathUtil.h"
#include <cmath>

namespace lf {

struct Atmosphere {
    Color zenith, horizon, sun, ambient, cloud;
    Vec3  sunDir;          // unit vector pointing TO the sun
    float sunStrength = 1;
    float progress = 0;
    float lampGlow = 0;    // 0..1 how strongly the pier lantern shines

    static Atmosphere at(float p) {
        struct Key { float t; Color zen, hor, sun, amb; };
        static const Key keys[] = {
            {0.00f, Color(.52f,.39f,.64f), Color(1,.69f,.48f), Color(1,.78f,.48f), Color(.44f,.42f,.49f)},
            {0.25f, Color(0.18f, 0.42f, 0.84f), Color(0.72f, 0.88f, 0.98f), Color(1.00f, 0.97f, 0.90f), Color(0.52f, 0.55f, 0.60f)},
            {0.55f, Color(0.49f, 0.43f, 0.72f), Color(1.00f, 0.72f, 0.53f), Color(1.00f, 0.88f, 0.66f), Color(0.50f, 0.48f, 0.50f)},
            {0.80f, Color(0.25f, 0.30f, 0.62f), Color(1.00f, 0.64f, 0.36f), Color(1.00f, 0.64f, 0.34f), Color(0.42f, 0.36f, 0.40f)},
            {1.00f, Color(0.12f, 0.12f, 0.34f), Color(0.92f, 0.40f, 0.30f), Color(0.95f, 0.40f, 0.22f), Color(0.30f, 0.24f, 0.34f)},
        };
        p = clampf(p, 0.0f, 1.0f);
        int i = 0;
        while (i < 3 && p > keys[i + 1].t) ++i;
        float t = (p - keys[i].t) / (keys[i + 1].t - keys[i].t);
        t=t*t*(3-2*t); // smooth colour handoffs at keyframes
        Atmosphere a;
        a.progress = p;
        a.zenith = lerpc(keys[i].zen, keys[i + 1].zen, t);
        a.horizon = lerpc(keys[i].hor, keys[i + 1].hor, t);
        a.sun = lerpc(keys[i].sun, keys[i + 1].sun, t);
        a.ambient = lerpc(keys[i].amb, keys[i + 1].amb, t);
        a.cloud = lerpc(Color(1, 1, 1), Color(1.0f, 0.66f, 0.55f), smoothstep(0.5f, 1.0f, p));

        // The sun sets in front of the angler, slightly to the right.
        float sunset = clampf((p-0.48f)/0.52f,0,1);
        float elevation=p<.25f?lerpf(4.f,35.f,p/.25f):p<.48f?lerpf(35.f,14.f,(p-.25f)/.23f):lerpf(14.f,-3.f,sunset);
        float elev=deg2rad(elevation);
        float az = deg2rad(lerpf(6.0f, -3.0f, p));
        a.sunDir = Vec3(std::sin(az) * std::cos(elev), std::sin(elev), -std::cos(az) * std::cos(elev)).normalized();
        a.sunStrength = 0.25f + 0.75f * smoothstep(-0.05f, 0.30f, a.sunDir.y);
        a.lampGlow = smoothstep(0.45f, 0.85f, p);
        return a;
    }
};

} // namespace lf
