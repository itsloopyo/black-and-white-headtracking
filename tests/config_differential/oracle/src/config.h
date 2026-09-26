#pragma once

#include <cstdint>
#include <string>

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace headtracking {

struct Config {
    uint16_t port = 4242;
    bool enabled_on_startup = true;

    float sens_yaw = 1.0f;
    float sens_pitch = 1.0f;
    float sens_roll = 1.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;

    // Smoothing is chosen per connection from the packet's source address, and
    // both values cover rotation and position alike. A tracker running on this
    // machine is already steady, so local_smoothing is 0.0 and nothing floors
    // it; a phone on WiFi jitters over the network, which is what
    // remote_smoothing is for.
    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    float deadzone_yaw = 0.0f;
    float deadzone_pitch = 0.0f;
    float deadzone_roll = 0.0f;

    // Positional (6DOF) tracking. Head displacement is applied camera-local as
    // the final translation on the render matrix - see camera_hook.cpp.
    bool  pos_enabled    = true;
    float pos_sens_x     = 1.0f;
    float pos_sens_y     = 1.0f;
    float pos_sens_z     = 1.0f;
    bool  pos_invert_x   = false;
    bool  pos_invert_y   = false;
    bool  pos_invert_z   = false;
    // Head-movement envelope in metres (clamped before world scaling). Z is
    // asymmetric: pos_limit_z = forward lean (generous), z_back = backward.
    float pos_limit_x      = cameraunlock::PositionSettings{}.limit_x;
    float pos_limit_y      = cameraunlock::PositionSettings{}.limit_y;
    float pos_limit_z      = cameraunlock::PositionSettings{}.limit_z;
    float pos_limit_z_back = cameraunlock::PositionSettings{}.limit_z_back;
    // Engine world units per metre of head movement. This is the primary
    // tuning knob - raise it until lean feels responsive without nausea.
    float pos_world_scale  = 40.0f;
    // Camera focal distance (zoom) at which pos_world_scale is calibrated.
    // The offset scales by focal/pos_zoom_reference so the on-screen effect is
    // constant across zoom. 0 = auto: lock to the first gameplay zoom seen.
    float pos_zoom_reference = 0.0f;
    // Clamp on the zoom multiplier, applied as [1/max, max]. B&W's focal
    // distance spans ~500x across the zoom range, so uncapped scaling lunges
    // the camera at full zoom-out. Lower this if zoom-out feels too strong.
    float pos_zoom_scale_max = 2.5f;

    int toggle_vk    = 0x23;  // VK_END
    int yaw_mode_vk  = 0x22;  // VK_NEXT (Page Down)
    int mode_cycle_vk = 0x21; // VK_PRIOR (Page Up): 6DOF -> rotation -> position
    int debounce_ms  = 200;

    // true  = horizon-locked yaw (yaw around world up axis, default)
    // false = camera-local yaw (yaw composed with pitch/roll)
    bool world_space_yaw = true;

    // Per-second trace of tracker metres, clamped metres, engine offset and
    // focal distance. Off by default: it is a tuning aid for WorldScale and
    // ZoomReference, and left on it buries the startup chain a bug report is
    // actually read for.
    bool log_position_trace = false;

    static std::string IniPath();  // <game folder>\HeadTracking.ini
    static Config LoadOrCreateDefault();
    static void WriteDefault(const std::string& path);
};

}  // namespace headtracking
