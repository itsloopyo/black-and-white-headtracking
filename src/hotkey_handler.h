#pragma once

#include "cameraunlock/input/hotkey_poller.h"

namespace headtracking {

class Plugin;
struct Config;

class HotkeyHandler {
public:
    // Registers the three key lists from `config` on the poller and starts it.
    void Start(Plugin& plugin, const Config& config);
    void Stop();

private:
    cameraunlock::input::HotkeyPoller m_poller;
};

}  // namespace headtracking
