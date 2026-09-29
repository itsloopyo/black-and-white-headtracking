#pragma once

#include "cameraunlock/camera/lean_clamp.h"

// The engine half of the lean collision clamp: how far the eye may travel from
// where the game put it before it leaves the space the game's own camera is
// allowed in. Core owns what to do with the answer (cameraunlock/camera/
// lean_clamp.h); this owns getting one.
//
// B&W's camera collides with two things and nothing else: the landscape, and
// the camera volumes the camera modes register (see engine_addresses.h). The
// query asks the engine about both, through the same functions its camera uses,
// so a lean never takes the eye anywhere the game's camera could not go.
//
// The standoff lives here rather than in the clamp, the way a sweep radius does:
// the eye is held that far above the land, and the camera volumes already come
// widened by the engine's camera radius. So the clamp's own skin must be 0.
namespace headtracking::lean_trace {

// How far above the land the eye is held, in world units. The query never holds
// it closer than the near plane reaches, which it reads from the engine on every
// call, since geometry inside that is clipped away and the land goes see-through.
void SetStandoff(float worldUnits);

// Distance from the eye to a corner of the near plane, read live from the engine.
float NearPlaneReach();

// The height the query keeps the eye above the land: the standoff, or the near
// plane's reach where that is larger.
float Clearance();

// How far `p` is above the land under it, from the engine's own height lookup.
float HeightAboveLand(const cameraunlock::math::Vec3& p);

// True when the last query started inside a camera volume, so it tested the land
// alone: a point test cannot say which way is out.
bool LastStartedInsideVolume();

// The query, in the shape core's clamp takes. `start` is the clean eye in world
// space. Must be called on the game thread, which owns the volume list.
cameraunlock::camera::LeanObstruction Query(void* context,
                                            const cameraunlock::math::Vec3& start,
                                            const cameraunlock::math::Vec3& direction,
                                            float maxDistance);

}  // namespace headtracking::lean_trace
