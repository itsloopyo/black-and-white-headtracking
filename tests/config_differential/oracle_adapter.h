#pragma once

// The oracle: the config reader and hotkey registration of v0.2.1, the newest published build,
// compiled from oracle/ with the core sources they included at its pin (c480d8a). Two libraries
// build it, each with its namespaces renamed at compile time so it links beside the current
// core: the reader, and HotkeyHandler::Start against oracle_fake's recording poller and counting
// Plugin. This header names no core type, so the test includes it without the renaming.

#include <array>
#include <string>
#include <vector>

namespace bw_oracle_view {

struct OracleConfig {
    int port;
    bool enabled_on_startup;
    float sens_yaw, sens_pitch, sens_roll;
    bool invert_yaw, invert_pitch, invert_roll;
    float local_smoothing, remote_smoothing;
    float deadzone_yaw, deadzone_pitch, deadzone_roll;
    bool pos_enabled;
    float pos_sens_x, pos_sens_y, pos_sens_z;
    bool pos_invert_x, pos_invert_y, pos_invert_z;
    float pos_limit_x, pos_limit_y, pos_limit_z, pos_limit_z_back;
    float pos_world_scale, pos_zoom_reference, pos_zoom_scale_max;
    int toggle_vk, yaw_mode_vk, mode_cycle_vk, debounce_ms;
    bool world_space_yaw;
    bool log_position_trace;
};

// The file v0.2.1 reads: HeadTracking.ini beside the running executable, as its Config::IniPath
// builds it. The oracle cannot be pointed anywhere else, so the test places each input there.
std::string OraclePath();

// Config::LoadOrCreateDefault as v0.2.1's Plugin::Initialize ran it. It creates the file when
// there is none, as that build did. v0.2.1 never refused a file.
OracleConfig RunOracle();

// Which actions a key press fires, for every key a binding can name (0x01-0xFE) under every set
// of held modifiers. Entry (vk - kFirstKey) * kHeldStates + held counts the toggle, cycle and
// yaw mode actions fired, in that order. held: 1 Ctrl, 2 Shift, 4 Alt.
constexpr int kFirstKey = 0x01;
constexpr int kLastKey = 0xFE;
constexpr int kHeldStates = 8;
using FireTable = std::vector<std::array<int, 3>>;

// v0.2.1's HotkeyHandler::Start run on the three codes, pressing each key under each held set.
FireTable OracleFires(int toggle_vk, int yaw_mode_vk, int mode_cycle_vk);

}  // namespace bw_oracle_view
