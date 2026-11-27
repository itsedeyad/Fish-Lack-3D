#include "core/World.h"
#include <cmath>

namespace world {

using namespace lf;

static lf::MapId current=lf::MapId::Forest;
const MapSpec& mapSpec(lf::MapId id) {
    static const MapSpec maps[]={
        {lf::MapId::Forest,"Forest Lake",0,101,50,40,4.5f,0,22,52,1,Color(.025f,.37f,.56f),Color(.14f,.77f,.74f),{1,1,1,1,1,1}},
        {lf::MapId::Highland,"Highland Lake",350,202,42,48,6.5f,-7,25,60,.7f,Color(.025f,.24f,.45f),Color(.18f,.62f,.81f),{1.15f,1.6f,.8f,1,.65f,1.4f}},
        {lf::MapId::Marsh,"Willow Marsh",600,303,56,34,3,9,17,47,.35f,Color(.11f,.28f,.18f),Color(.36f,.57f,.28f),{.8f,.65f,1.5f,1.25f,1.7f,.8f}}
    };
    int i=int(id);return maps[i>=0 && i<3?i:0];
}
lf::MapId activeMap(){return current;}
void setMap(lf::MapId id) {
    const auto& m=mapSpec(id);current=m.id;LAKE_RX=m.rx;LAKE_RZ=m.rz;LAKE_DEPTH=m.depth;
    PIER_X=m.pierX;PIER_Z_START=m.pierStart;PIER_Z_END=m.pierEnd;
}

float lakeDistance(float x, float z) {
    float ang = std::atan2(z, x);
    // A few low-frequency sine terms bend the ellipse into an organic shoreline.
    float wobble = 1.0f + 0.07f * std::sin(3.0f * ang + (0.6f+int(current)*.7f))
                        + 0.045f * std::sin(5.0f * ang + 2.1f)
                        + 0.03f * std::sin(9.0f * ang + 4.0f);
    float ex = x / LAKE_RX, ez = z / LAKE_RZ;
    return std::sqrt(ex * ex + ez * ez) / wobble;
}

float terrainHeight(float x, float z) {
    float d = lakeDistance(x, z);
    float h;
    if (d < 1.0f) {
        // Lake bed: 0 at the shoreline, falling quickly then flattening out.
        float t = clampf((1.0f - d) / 0.55f, 0.0f, 1.0f);
        float bowl = 1.0f - (1.0f - t) * (1.0f - t);          // ease-out
        h = -LAKE_DEPTH * bowl;
        h += (fbm(x * 0.08f, z * 0.08f, 3, 7) - 0.5f) * 0.8f * t; // bumpy bed
    } else {
        float s = d - 1.0f;                                     // distance past shore
        // Sandy beach slope, then rolling hills, then a ring of higher ground.
        h = s * 9.0f;
        float hills = (fbm(x * 0.018f, z * 0.018f, 4, 3) - 0.35f) * 16.0f;
        h += std::fmax(hills, -1.0f) * smoothstep(0.08f, 0.55f, s);
        h += 14.0f * smoothstep(0.6f, 2.2f, s);
        h += (fbm(x * 0.15f, z * 0.15f, 2, 11) - 0.5f) * 0.6f * smoothstep(0.02f, 0.2f, s);
    }

    if(d>=1 && current==lf::MapId::Highland) {
        h=std::floor(h/2.2f)*2.2f + 2* smoothstep(1.1f,1.6f,d);
    } else if(d>=1 && current==lf::MapId::Marsh) {
        h*=.30f;h+=.55f*std::sin(x*.12f)*std::sin(z*.09f)*smoothstep(1,1.3f,d);
    }
    // A low northern valley keeps the actual sunset visible from the dock.
    // This only affects land, so the castable lake and fish collision surface are unchanged.
    if(d>=1.0f) {
        float valley=(1-smoothstep(11.f,29.f,std::fabs(x-PIER_X)))*(1-smoothstep(-LAKE_RZ-25,-LAKE_RZ,z));
        float lowland=.65f+.25f*fbm(x*.05f,z*.05f,2,42);
        h=lerpf(h,lowland,valley);
    }

    // Flatten a strip under and behind the pier so the walkway meets the land.
    float ax = std::fabs(x - PIER_X);
    float f = (1.0f - smoothstep(3.0f, 9.0f, ax)) * smoothstep(PIER_Z_START + 14.0f, PIER_Z_START + 20.0f, z);
    if (f > 0.0f) {
        float pathH = (z < PIER_Z_END) ? 0.95f : 0.95f + (z - PIER_Z_END) * 0.12f;
        if (h > pathH) h = lerpf(h, pathH, f);
    }
    return h;
}

Vec3 terrainNormal(float x, float z) {
    const float e = 0.5f;
    float hl = terrainHeight(x - e, z), hr = terrainHeight(x + e, z);
    float hd = terrainHeight(x, z - e), hu = terrainHeight(x, z + e);
    return Vec3(hl - hr, 2.0f * e, hd - hu).normalized();
}

// Each wave: amplitude, wave-vector (kx, kz) and angular speed.
struct Wave { float a, kx, kz, w; };
static const Wave kWaves[] = {
    {0.060f, 0.35f, 0.10f, 1.10f},
    {0.045f, -0.12f, 0.30f, 0.90f},
    {0.025f, 0.62f, 0.58f, 2.00f},
    {0.016f, -0.95f, 0.70f, 2.70f},
    {0.010f, 1.40f, -1.10f, 3.60f},
};

float waveHeight(float x, float z, float t) {
    float h = 0;
    for (const Wave& w : kWaves) h += mapSpec(current).waveScale * w.a * std::sin(w.kx * x + w.kz * z + w.w * t);
    return WATER_LEVEL + h;
}

Vec3 waveNormal(float x, float z, float t) {
    // n = (-dh/dx, 1, -dh/dz), derivative of a*sin(kx*x + kz*z + w*t)
    float dx = 0, dz = 0;
    for (const Wave& w : kWaves) {
        float c = mapSpec(current).waveScale * w.a * std::cos(w.kx * x + w.kz * z + w.w * t);
        dx += c * w.kx;
        dz += c * w.kz;
    }
    return Vec3(-dx, 1.0f, -dz).normalized();
}

bool isWater(float x, float z, float minDepth) {
    return terrainHeight(x, z) < WATER_LEVEL - minDepth;
}

bool insidePier(float x, float z, float margin) {
    return std::fabs(x - PIER_X) < PIER_HALF_W + margin &&
           z > PIER_Z_START - margin && z < PIER_Z_END + margin;
}

} // namespace world
