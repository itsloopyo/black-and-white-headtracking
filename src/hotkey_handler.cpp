#include "hotkey_handler.h"
#include "plugin.h"
#include "debug_log.h"

#include "cameraunlock/input/chord_hotkeys.h"

namespace headtracking {

namespace {
// Ctrl+Shift chord letters per the shared T/Y/U/G/H/J cluster convention:
// T = recenter, Y = toggle tracking, G = mode cycle, H = yaw mode.
constexpr int kVkT = 0x54;
constexpr int kVkY = 0x59;
constexpr int kVkG = 0x47;
constexpr int kVkH = 0x48;
}  // namespace

void HotkeyHandler::Start(Plugin& plugin, int recenter_vk, int toggle_vk, int yaw_mode_vk,
                          int mode_cycle_vk) {
    using cameraunlock::input::ChordGuarded;

    const auto recenter = [&plugin]() {
        plugin.Recenter();
        HT_LOG("[hotkey] recenter");
    };
    const auto toggle = [&plugin]() {
        plugin.ToggleEnabled();
        HT_LOG("[hotkey] toggle -> %s", plugin.IsEnabled() ? "on" : "off");
    };
    const auto yawMode = [&plugin]() {
        plugin.ToggleYawMode();
        HT_LOG("[hotkey] yaw mode -> %s",
               plugin.IsWorldSpaceYaw() ? "world-space" : "camera-local");
    };
    const auto modeCycle = [&plugin]() {
        plugin.CycleTrackingMode();
        HT_LOG("[hotkey] mode cycle -> %s", plugin.TrackingModeName());
    };

    m_poller.SetRecenterKey(recenter_vk, recenter);
    m_poller.SetToggleKey(toggle_vk, toggle);
    m_poller.AddHotkey(yaw_mode_vk, yawMode);
    m_poller.AddHotkey(mode_cycle_vk, modeCycle);

    m_poller.AddHotkey(kVkT, ChordGuarded(recenter));
    m_poller.AddHotkey(kVkY, ChordGuarded(toggle));
    m_poller.AddHotkey(kVkH, ChordGuarded(yawMode));
    m_poller.AddHotkey(kVkG, ChordGuarded(modeCycle));

    m_poller.Start(16);
}

void HotkeyHandler::Stop() {
    m_poller.Stop();
}

}  // namespace headtracking
