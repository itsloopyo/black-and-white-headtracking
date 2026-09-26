#pragma once

// Stands in for v0.2.1's Plugin in the hotkey oracle library only. HotkeyHandler::Start takes
// the plugin by reference and its three actions call ToggleEnabled, CycleTrackingMode and
// ToggleYawMode; this one counts those calls and answers the log lines' questions.

#include <array>

namespace headtracking {

class Plugin {
public:
    // Calls to ToggleEnabled, CycleTrackingMode and ToggleYawMode, in that order.
    std::array<int, 3> fired{};

    bool IsEnabled() const { return false; }
    void ToggleEnabled() { ++fired[0]; }
    bool IsWorldSpaceYaw() const { return true; }
    void ToggleYawMode() { ++fired[2]; }
    void CycleTrackingMode() { ++fired[1]; }
    const char* TrackingModeName() const { return ""; }
};

}  // namespace headtracking
