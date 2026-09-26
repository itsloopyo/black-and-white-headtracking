#pragma once

// The config reader of v0.2.1, the last build that read HeadTracking.ini, frozen so a player
// updating from any older build is converted exactly as that build read the file. Nothing in
// this folder is ever edited. Three things differ from the reader it was taken from: it fills
// this frozen copy of that build's Config and defaults rather than the runtime type, it never
// writes the file (a missing file reads as the defaults, which is what the old reader read from
// the file it created there), and it reports an absent file apart from one it read. The core
// default values the old Config took from PositionSettings and smoothing_utils.h are written
// out here as numbers, so a later core cannot move what an old file converts to.

#include "cameraunlock/config/legacy_import.h"

#include <cstdint>
#include <vector>

namespace headtracking::legacy {

enum class ReadStatus {
    Read,
    // No file at the path, or none the old reader could open. Config holds the defaults.
    Absent,
};

struct Config {
    uint16_t port = 4242;
    bool enabled_on_startup = true;

    float sens_yaw = 1.0f;
    float sens_pitch = 1.0f;
    float sens_roll = 1.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;

    float local_smoothing = 0.0f;
    float remote_smoothing = 0.15f;

    float deadzone_yaw = 0.0f;
    float deadzone_pitch = 0.0f;
    float deadzone_roll = 0.0f;

    bool  pos_enabled    = true;
    float pos_sens_x     = 1.0f;
    float pos_sens_y     = 1.0f;
    float pos_sens_z     = 1.0f;
    bool  pos_invert_x   = false;
    bool  pos_invert_y   = false;
    bool  pos_invert_z   = false;
    float pos_limit_x      = 0.30f;
    float pos_limit_y      = 0.20f;
    float pos_limit_z      = 0.40f;
    float pos_limit_z_back = 0.10f;
    float pos_world_scale  = 40.0f;
    float pos_zoom_reference = 0.0f;
    float pos_zoom_scale_max = 2.5f;

    int toggle_vk    = 0x23;  // VK_END
    int yaw_mode_vk  = 0x22;  // VK_NEXT (Page Down)
    int mode_cycle_vk = 0x21; // VK_PRIOR (Page Up)
    int debounce_ms  = 200;

    bool world_space_yaw = true;

    bool log_position_trace = false;
};

// Reads the file at `path`, the ANSI path v0.2.1 opened it by, into a default-constructed `c`.
ReadStatus Read(const char* path, Config& c);

// Every section and key Read reads, in the order it reads them.
std::vector<cameraunlock::config::LegacyKey> ReadKeys();

}  // namespace headtracking::legacy
