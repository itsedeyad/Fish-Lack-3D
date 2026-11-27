#include "game/Fish.h"
#include "core/World.h"
#include <cmath>

namespace lf {

// Path validity: the whole ellipse must be in water at least 1.4 m deep and
// away from the pier, so a cruising fish never touches the shore.
void FishSchool::makePath(Fish& f, Rng& rng) {
    for (int attempt = 0; attempt < 80; ++attempt) {
        Vec3 c(rng.range(-world::LAKE_RX * 0.6f, world::LAKE_RX * 0.6f), 0,
               rng.range(-world::LAKE_RZ * 0.6f, world::LAKE_RZ * 0.55f));
        float rx = rng.range(4.0f, 13.0f), rz = rng.range(3.0f, 9.0f);
        float tilt = rng.range(0.0f, PI);
        bool ok = true;
        for (int k = 0; k < 20 && ok; ++k) {
            float a = TWO_PI * k / 20.0f;
            float lx = rx * std::cos(a), lz = rz * std::sin(a);
            float x = c.x + lx * std::cos(tilt) - lz * std::sin(tilt);
            float z = c.z + lx * std::sin(tilt) + lz * std::cos(tilt);
            if (!world::isWater(x, z, 1.4f) || world::insidePier(x, z, 2.5f)) ok = false;
        }
        if (ok) {
            f.pathCenter = c; f.pathRx = rx; f.pathRz = rz; f.pathTilt = tilt;
            return;
        }
    }
    // Fallback: small circle in the middle of the lake (always deep water).
    f.pathCenter = Vec3(rng.range(-8.f, 8.f), 0, rng.range(-8.f, 4.f));
    f.pathRx = 5; f.pathRz = 4; f.pathTilt = 0;
}

Vec3 FishSchool::pathPoint(const Fish& f, float param) const {
    float lx = f.pathRx * std::cos(param);
    float lz = f.pathRz * std::sin(param);
    float c = std::cos(f.pathTilt), s = std::sin(f.pathTilt);
    return {f.pathCenter.x + lx * c - lz * s, 0.0f, f.pathCenter.z + lx * s + lz * c};
}

void FishSchool::respawn(Fish& f, Rng& rng, int progression) {
    // Later in the game the heavier, more valuable (and harder) fish become
    // more common: this is the difficulty progression of the fish population.
    int level=std::min(6,progression);
    float weights[]={5.f,3.f+level*.4f,1.6f+level*.7f,4.f,1.4f+level*.2f,.6f+level*.08f};
    for(int i=0;i<FISH_TYPE_COUNT;++i)weights[i]*=world::mapSpec(world::activeMap()).fishWeight[i];
    float total=0;for(float weight:weights)total+=weight;
    float pick=rng.range(0,total);f.type=FISH_TYPE_COUNT-1;
    for(int i=0;i<FISH_TYPE_COUNT;++i){pick-=weights[i];if(pick<=0){f.type=i;break;}}
    constexpr float poisonChance=.08f,healthyMarkingChance=.12f;
    f.toxic=rng.chance(poisonChance);
    // Keep the short circuit: toxic fish must not consume a second RNG sample.
    f.suspicious=f.toxic || rng.chance(healthyMarkingChance);
    const FishSpec& sp = fishSpec(f.type);
    makePath(f, rng);
    f.mode = FishMode::Cruise;
    f.pathParam = rng.range(0.0f, TWO_PI);
    f.pathDir = rng.chance(0.5f) ? 1.0f : -1.0f;
    float avgR = 0.5f * (f.pathRx + f.pathRz);
    f.speed = sp.speed * rng.range(0.8f, 1.1f);
    f.pathSpeed = f.speed / avgR;
    f.depth = sp.depth * rng.range(0.8f, 1.2f);
    f.sizeScale = rng.range(0.88f, 1.12f);
    f.pos = pathPoint(f, f.pathParam);
    f.pos.y=std::max(-f.depth,world::terrainHeight(f.pos.x,f.pos.z)+.35f);
    Vec3 ahead = pathPoint(f, f.pathParam + 0.3f * f.pathDir);
    f.yaw = std::atan2(ahead.x - f.pos.x, ahead.z - f.pos.z);
    f.cooldown = rng.range(0.0f, 3.0f);
    f.tailPhase = rng.range(0.0f, TWO_PI);
    f.pitch = f.roll = 0;
}

void FishSchool::init(Rng& rng) {
    fish.clear();
    // Initial population: mostly small fish, a few big ones.
    // Guaranteed representatives, then weighted species. Bounded population: 24.
    int types[24];for(int i=0;i<24;++i)types[i]=i<FISH_TYPE_COUNT?i:-1;
    int id = 0;
    for (int t : types) {
        Fish f;
        f.id = id++;
        respawn(f, rng, 0);
        if(t>=0)f.type=t;
        const FishSpec& sp = fishSpec(f.type);
        f.speed = sp.speed * rng.range(0.8f, 1.1f);
        f.pathSpeed = f.speed / (0.5f * (f.pathRx + f.pathRz));
        f.depth = sp.depth * rng.range(0.8f, 1.2f);
        f.pos.y=std::max(-f.depth,world::terrainHeight(f.pos.x,f.pos.z)+.35f);
        fish.push_back(f);
    }
}

void FishSchool::steer(Fish& f, const Vec3& target, float targetSpeed, float dt) {
    const FishSpec& sp = fishSpec(f.type);
    Vec3 d = target - f.pos;
    float desired = std::atan2(d.x, d.z);
    float diff = wrapAngle(desired - f.yaw);
    float maxTurn = sp.turnRate * dt;
    float turn = clampf(diff, -maxTurn, maxTurn);
    f.yaw = wrapAngle(f.yaw + turn);
    f.turnRate = lerpf(f.turnRate, dt > 0 ? turn / dt : 0.0f, damp(8.0f, dt));
    f.speed = lerpf(f.speed, targetSpeed, damp(2.0f, dt));

    Vec3 next = f.pos + yawDir(f.yaw) * (f.speed * dt);
    // Stay inside the lake: if the next step would leave deep water (or hit
    // the pier) turn firmly towards the lake centre and slow down.
    if (!world::isWater(next.x, next.z, 1.0f) || world::insidePier(next.x, next.z, 0.7f)) {
        float toCenter = std::atan2(-f.pos.x, -f.pos.z);
        f.yaw = wrapAngle(f.yaw + clampf(wrapAngle(toCenter - f.yaw), -3.0f * dt, 3.0f * dt));
        next = f.pos + yawDir(f.yaw) * (f.speed * dt * 0.3f);
        if (!world::isWater(next.x, next.z, 0.8f) || world::insidePier(next.x, next.z, 0.5f)) next = f.pos;
    }
    f.pos.x = next.x;
    f.pos.z = next.z;
}

void FishSchool::update(float dt, float time, Rng& rng, int progression) {
    for (Fish& f : fish) {
        const FishSpec& sp = fishSpec(f.type);
        f.modeTimer += dt;
        if (f.cooldown > 0) f.cooldown -= dt;
        float targetDepth = f.depth;

        switch (f.mode) {
        case FishMode::Cruise: {
            f.pathParam += f.pathSpeed * f.pathDir * dt;
            Vec3 target = pathPoint(f, f.pathParam + 0.5f * f.pathDir);
            float wander = 0.85f + 0.25f * std::sin(time * 0.4f + f.id * 1.7f);
            steer(f, target, sp.speed * wander, dt);
            break;
        }
        case FishMode::Approach: {
            float dist = distXZ(f.pos, bobberTarget);
            float spd = std::fmin(sp.speed * 0.8f, 0.35f + dist * 0.6f);
            steer(f, bobberTarget, spd, dt);
            targetDepth = 0.55f;
            break;
        }
        case FishMode::Nibble: {
            // Circle slowly around the bait, nose pointing at it.
            f.orbitAngle += 0.9f * dt;
            Vec3 t = bobberTarget + Vec3(std::cos(f.orbitAngle) * 0.9f, 0, std::sin(f.orbitAngle) * 0.9f);
            steer(f, t, 0.45f, dt);
            targetDepth = 0.45f;
            break;
        }
        case FishMode::Bite:
            steer(f, bobberTarget, 0.6f, dt);
            targetDepth = 0.35f;
            break;
        case FishMode::Hooked:
        case FishMode::Landed:
            // Position/orientation are driven by the FishingSystem.
            f.tailPhase += dt * 22.0f;
            f.tailAmp = lerpf(f.tailAmp, 0.75f, damp(6.0f, dt));
            continue;
        case FishMode::Flee: {
            steer(f, f.fleeTarget, sp.speed * 2.0f, dt);
            if (f.modeTimer > 3.5f || distXZ(f.pos, f.fleeTarget) < 1.5f) {
                f.mode = FishMode::Cruise;
                f.modeTimer = 0;
                // re-join the path at the closest parameter (approximate)
                Vec3 rel = f.pos - f.pathCenter;
                f.pathParam = std::atan2(rel.z, rel.x) - f.pathTilt;
            }
            break;
        }
        case FishMode::Inactive:
            f.respawnTimer -= dt;
            if (f.respawnTimer <= 0) respawn(f, rng, progression);
            continue;
        }

        // Gentle depth change plus a slow vertical undulation.
        float wantY = -targetDepth + 0.12f * std::sin(time * 0.8f + f.id);
        float bed = world::terrainHeight(f.pos.x, f.pos.z) + 0.35f;
        if (wantY < bed) wantY = bed;
        f.pos.y = std::max(bed,lerpf(f.pos.y, wantY, damp(1.5f, dt)));

        // Tail: beats faster when swimming fast and harder while turning.
        f.tailPhase += dt * (3.5f + f.speed * 5.0f);
        float wantAmp = 0.22f + f.speed * 0.10f + std::fabs(f.turnRate) * 0.25f;
        f.tailAmp = lerpf(f.tailAmp, clampf(wantAmp, 0.15f, 0.7f), damp(4.0f, dt));
        f.roll = lerpf(f.roll, clampf(-f.turnRate * 0.25f, -0.4f, 0.4f), damp(4.0f, dt));
        f.pitch = lerpf(f.pitch, 0.0f, damp(3.0f, dt));
    }
}

int FishSchool::findCandidate(const Vec3& bobber, float radius,int preferredA,int preferredB) const {
    int best = -1;
    float bestD = radius;
    for (size_t i = 0; i < fish.size(); ++i) {
        const Fish& f = fish[i];
        if (f.mode != FishMode::Cruise || f.cooldown > 0) continue;
        float d = distXZ(f.pos, bobber);
        if(d>=radius)continue;
        float priority=d / ((f.type==preferredA || f.type==preferredB)?2.f:1.f);
        if (priority < bestD) { bestD = priority; best = int(i); }
    }
    return best;
}

void FishSchool::startApproach(int i) { fish[i].mode = FishMode::Approach; fish[i].modeTimer = 0; }
void FishSchool::startNibble(int i) {
    Fish& f = fish[i];
    f.mode = FishMode::Nibble;
    f.modeTimer = 0;
    f.orbitAngle = std::atan2(f.pos.z - bobberTarget.z, f.pos.x - bobberTarget.x);
}
void FishSchool::startBite(int i) { fish[i].mode = FishMode::Bite; fish[i].modeTimer = 0; }
void FishSchool::setHooked(int i) { fish[i].mode = FishMode::Hooked; fish[i].modeTimer = 0; }
void FishSchool::startLanding(int i) { fish[i].mode = FishMode::Landed; fish[i].modeTimer = 0; }

void FishSchool::flee(int i, const Vec3& from) {
    Fish& f = fish[i];
    f.mode = FishMode::Flee;
    f.modeTimer = 0;
    f.cooldown = 10.0f;
    Vec3 away = f.pos - from;
    away.y = 0;
    if (away.lengthXZ() < 0.1f) away = Vec3(0, 0, -1);
    away = away.normalized();
    // Flee towards the open lake, pulled slightly towards the centre so the
    // target is always valid water.
    Vec3 t = f.pos + away * 12.0f;
    for (int k = 0; k < 10 && !world::isWater(t.x, t.z, 1.5f); ++k) t = lerpv(t, Vec3(0, 0, -5), 0.3f);
    f.fleeTarget = t;
    f.pitch = 0;
}

void FishSchool::deactivate(int i) {
    Fish& f = fish[i];
    f.mode = FishMode::Inactive;
    f.respawnTimer = 6.0f;
    f.pos = Vec3(0, -50, 0);
}

} // namespace lf
