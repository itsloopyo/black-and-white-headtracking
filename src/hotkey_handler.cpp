#include "hotkey_handler.h"
#include "config.h"
#include "plugin.h"
#include "debug_log.h"

#include <stdexcept>
#include <string>

#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"

namespace headtracking {

namespace {

// The table read every list through the hotkey codec, so a list that does not parse here is a
// bug, not a player's typo.
void Register(cameraunlock::input::HotkeyPoller& poller, const std::string& list, const char* key,
              std::function<void()> action) {
    const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(list);
    if (!parsed.ok()) {
        throw std::logic_error(std::string("[Hotkeys] ") + key + "=" + list + " does not parse: " + parsed.error);
    }
    cameraunlock::input::RegisterKeyBindings(poller, parsed.bindings, std::move(action));
}

}  // namespace

void HotkeyHandler::Start(Plugin& plugin, const Config& config) {
    Register(m_poller, config.toggle_key_name, "ToggleKey", [&plugin]() {
        plugin.ToggleEnabled();
        HT_LOG("[hotkey] toggle -> %s", plugin.IsEnabled() ? "on" : "off");
    });
    Register(m_poller, config.cycle_tracking_mode_key_name, "CycleTrackingModeKey", [&plugin]() {
        plugin.CycleTrackingMode();
    });
    Register(m_poller, config.yaw_mode_key_name, "YawModeKey", [&plugin]() {
        plugin.ToggleYawMode();
    });

    m_poller.Start(16);
}

void HotkeyHandler::Stop() {
    m_poller.Stop();
}

}  // namespace headtracking
