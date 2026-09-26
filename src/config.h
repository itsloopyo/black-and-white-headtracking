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

// Core's config with this game's own rows.
struct Config : cameraunlock::HeadTrackingConfig {
    // Camera focal distance (zoom) at which kWorldUnitsPerMetre is calibrated. The offset
    // scales by focal/pos_zoom_reference so the on-screen effect is constant across zoom.
    // 0 = auto: lock to the first gameplay zoom seen.
    float pos_zoom_reference = 0.0f;
    // Clamp on the zoom multiplier, applied as [1/max, max]. B&W's focal distance spans ~500x
    // across the zoom range, so uncapped scaling lunges the camera at full zoom-out.
    float pos_zoom_scale_max = 2.5f;

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
