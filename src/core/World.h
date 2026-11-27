// =============================================================================
//  World.h - shared description of the game world (pure math, no OpenGL).
//
//  The terrain is an analytic height function. The lake is the region where
//  the terrain dips below the water plane (y = 0). Because the logic (casting,
//  fish movement) and the renderer (terrain mesh) use the same functions, the
//  bobber always lands where the player sees water and fish never swim
//  through the ground.
// =============================================================================
#pragma once
#include "core/MathUtil.h"

namespace lf { enum class MapId { Forest, Highland, Marsh }; }

namespace world {

using lf::Vec3;

constexpr float WATER_LEVEL   = 0.0f;
inline float LAKE_RX = 50.0f;   // lake half-extent along X
inline float LAKE_RZ = 40.0f;   // lake half-extent along Z
inline float LAKE_DEPTH = 4.5f;    // deepest point of the lake bed
constexpr float TERRAIN_HALF  = 160.0f;  // terrain covers [-160,160]^2

// Pier: a wooden walkway along +Z from the shore out into the lake.
inline float PIER_X = 0.0f;
constexpr float PIER_HALF_W   = 1.9f;
inline float PIER_Z_START = 22.0f;   // lake-side end (where the angler stands)
inline float PIER_Z_END = 52.0f;   // land-side end
constexpr float PIER_DECK_Y   = 1.30f;   // top of the deck planks

struct MapSpec {
    lf::MapId id;const char* name;int price,seed;
    float rx,rz,depth,pierX,pierStart,pierEnd,waveScale;
    lf::Color deep,shallow;
    float fishWeight[6];
};
const MapSpec& mapSpec(lf::MapId id);
lf::MapId activeMap();
void setMap(lf::MapId id);
inline Vec3 pierPoint(float x,float y,float z) {return Vec3(x+PIER_X,y,z+PIER_Z_START-22);}

/// Where the angler stands (feet position, on the pier deck).
inline Vec3 anglerPosition() { return {PIER_X + 0.15f, PIER_DECK_Y, PIER_Z_START + 1.6f}; }

/// Normalised elliptical distance from the lake centre with a wobbly,
/// natural-looking outline. < 1 inside the lake, > 1 outside.
float lakeDistance(float x, float z);

/// Ground / lake-bed height at (x, z).
float terrainHeight(float x, float z);

/// Analytic surface normal of the terrain (central differences).
Vec3 terrainNormal(float x, float z);

/// Animated water surface height: a sum of travelling sine waves.
float waveHeight(float x, float z, float t);

/// Normal of the animated water surface (analytic derivative of waveHeight).
Vec3 waveNormal(float x, float z, float t);

/// True if (x,z) is open water at least `minDepth` deep.
bool isWater(float x, float z, float minDepth = 0.5f);

/// True if the point is under/inside the pier footprint (with margin).
bool insidePier(float x, float z, float margin = 0.0f);

} // namespace world
