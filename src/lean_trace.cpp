#include "lean_trace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "engine_addresses.h"

namespace headtracking::lean_trace {

namespace {

using cameraunlock::camera::LeanObstruction;
using cameraunlock::math::Vec3;

typedef float (__fastcall *Fn_LandHeight_t)(const int32_t* mapXZ);
typedef bool (__cdecl *Fn_InCameraVolume_t)(float x, float y, float z);

// The land is piecewise linear over 10-unit cells, so sampling every unit sees
// every change of slope a lean can cross. A lean is at most a few tens of units.
constexpr float kStep = 1.0f;
constexpr int kMaxSteps = 64;
constexpr int kBisections = 10;
// Slack on the height comparison, so a lean across flat ground that starts on
// the floor does not trip over float noise in the height lookup.
constexpr float kHeightSlack = 1e-3f;

float g_standoff = 0.0f;
bool g_startedInsideVolume = false;

// A position that does not fit the engine's fixed point is far off the map,
// where the engine's own answer is sea level.
bool ToMapFixed(float world, int32_t& out) {
    const double fixed = static_cast<double>(world) * kMapFixedPerWorldUnit;
    if (!(fixed > static_cast<double>(INT32_MIN) && fixed < static_cast<double>(INT32_MAX))) return false;
    out = static_cast<int32_t>(fixed);
    return true;
}

float LandHeight(float x, float z) {
    int32_t map[2];
    if (!ToMapFixed(x, map[0]) || !ToMapFixed(z, map[1])) return 0.0f;
    return reinterpret_cast<Fn_LandHeight_t>(kFn_LandHeight_Addr)(map);
}

bool InCameraVolume(const Vec3& p) {
    return reinterpret_cast<Fn_InCameraVolume_t>(kFn_InCameraVolume_Addr)(p.x, p.y, p.z);
}

}  // namespace

void SetStandoff(float worldUnits) { g_standoff = worldUnits; }

float NearPlaneReach() {
    const float nearZ = *reinterpret_cast<const float*>(kNearPlaneAddr);
    const float halfW = *reinterpret_cast<const float*>(kProjScaleXAddr);
    const float halfH = *reinterpret_cast<const float*>(kProjScaleYAddr);
    return std::sqrt(nearZ * nearZ + halfW * halfW + halfH * halfH);
}

float HeightAboveLand(const Vec3& p) { return p.y - LandHeight(p.x, p.z); }

float Clearance() { return std::max(g_standoff, NearPlaneReach()); }

bool LastStartedInsideVolume() { return g_startedInsideVolume; }

LeanObstruction Query(void* /*context*/, const Vec3& start, const Vec3& direction, float maxDistance) {
    // The game's camera can already sit lower than the clearance (its ground test
    // is a ring round the eye, not the eye itself). Holding the lean to the full
    // clearance there would refuse every lean, including one away from the
    // ground, so it is held no lower than where the game put the eye instead.
    const float startAbove = HeightAboveLand(start);
    const float floorAbove = std::min(Clearance(), startAbove) - kHeightSlack;
    g_startedInsideVolume = InCameraVolume(start);

    const auto blocked = [&](float t) {
        const Vec3 p = start + direction * t;
        if (HeightAboveLand(p) < floorAbove) return true;
        return !g_startedInsideVolume && InCameraVolume(p);
    };

    const int steps = std::clamp(static_cast<int>(std::ceil(maxDistance / kStep)), 1, kMaxSteps);
    float free = 0.0f;
    for (int i = 1; i <= steps; ++i) {
        const float t = maxDistance * static_cast<float>(i) / static_cast<float>(steps);
        if (!blocked(t)) {
            free = t;
            continue;
        }
        float hit = t;
        for (int b = 0; b < kBisections; ++b) {
            const float mid = 0.5f * (free + hit);
            if (blocked(mid)) hit = mid; else free = mid;
        }
        LeanObstruction result;
        result.queried = true;
        result.blocked = true;
        result.distance = free;
        return result;
    }
    LeanObstruction result;
    result.queried = true;
    result.blocked = false;
    return result;
}

}  // namespace headtracking::lean_trace
