#pragma once

#include "cameraunlock/input/hotkey_poller.h"

namespace headtracking {

class Plugin;

class HotkeyHandler {
public:
    void Start(Plugin& plugin, int recenter_vk, int toggle_vk, int yaw_mode_vk,
               int mode_cycle_vk);
    void Stop();

private:
    cameraunlock::input::HotkeyPoller m_poller;
};

}  // namespace headtracking
