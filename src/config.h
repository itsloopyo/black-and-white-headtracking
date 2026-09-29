#pragma once

#include <string>

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/config_table.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/head_tracking_config.h"
#include "cameraunlock/config/legacy_import.h"

namespace headtracking {

constexpr const char* kConfigFileName = "CameraUnlock.ini";
// The file every build before the canonical format read, beside kConfigFileName. Imported once
// while kConfigFileName is absent, and never written.
constexpr const char* kLegacyConfigFileName = "HeadTracking.ini";
// The game's name as cameraunlock-core's data/games.json spells it.
constexpr const char* kConfigDisplayName = "Black & White";

// Engine world units per metre of head movement. Every build before the canonical format read
// it from [Position] WorldScale, shipped and defaulted at 40, which the axis conversion now
// applies as a constant.
constexpr float kWorldUnitsPerMetre = 40.0f;

// The zoom scaling of kWorldUnitsPerMetre, part of the same unit conversion. The lean offset
// scales by focal/reference, the reference being the first gameplay zoom seen, so its on-screen
// effect is the same at every zoom, clamped to [1/kZoomScaleMax, kZoomScaleMax] because B&W's
// focal distance spans ~500x across the zoom range and uncapped scaling lunges the camera at full
// zoom-out. Every build before the canonical format read the reference from [Position]
// ZoomReference and the clamp from ZoomScaleMax, shipped and defaulted at 0 (the first zoom) and
// 2.5.
constexpr float kZoomScaleMax = 2.5f;

// [Position] CollisionMargin's default, in world units: the radius the game's own camera keeps
// between its eye and what it collides with (the float at 0x009CE618), so a lean holds the eye
// off the land exactly as far as the game already does.
constexpr float kDefaultCollisionMargin = 3.0f;

// Core's config with this game's own rows.
struct Config : cameraunlock::HeadTrackingConfig {
    Config() { lean_clamp.skin = kDefaultCollisionMargin; }

    // Per-second trace of tracker metres, clamped metres, engine offset and focal distance.
    bool log_position_trace = false;
};

// The rows of CameraUnlock.ini. Only the tracking mode pair and WorldSpaceYaw are Writable: the
// mode and yaw hotkeys save the player's choice, and End changes the session only.
cameraunlock::config::ConfigTable<Config> MakeConfigTable();

// HeadTracking.ini as v0.2.1 read it (legacy_config/), mapped into Config.
cameraunlock::config::LegacyImport<Config> MakeLegacyImport();

// The owner's options for the files in `folder` (with its trailing separator): the settings in
// CameraUnlock.ini, imported once from HeadTracking.ini. The mod passes DefaultsFile::PerUser()
// and a test a scratch file.
cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults);

}  // namespace headtracking
