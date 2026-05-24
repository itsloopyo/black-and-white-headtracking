#include "hotkey_handler.h"
#include "plugin.h"
#include "debug_log.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace headtracking {

namespace {
constexpr int kVkT = 0x54;
constexpr int kVkY = 0x59;
constexpr int kVkG = 0x47;
constexpr int kVkH = 0x48;

bool CtrlShiftHeld() {
#ifdef _WIN32
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0
        && (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
#else
    return false;
#endif
}
}  // namespace

void HotkeyHandler::Start(Plugin& plugin, int recenter_vk, int toggle_vk, int yaw_mode_vk,
                          int mode_cycle_vk) {
    m_poller.SetRecenterKey(recenter_vk, [&plugin]() {
        plugin.Recenter();
        HT_LOG("[hotkey] recenter");
    });
    m_poller.SetToggleKey(toggle_vk, [&plugin]() {
        plugin.ToggleEnabled();
        HT_LOG("[hotkey] toggle -> %s", plugin.IsEnabled() ? "on" : "off");
    });
    m_poller.AddHotkey(yaw_mode_vk, [&plugin]() {
        plugin.ToggleYawMode();
        HT_LOG("[hotkey] yaw mode -> %s",
               plugin.IsWorldSpaceYaw() ? "world-space" : "camera-local");
    });
    m_poller.AddHotkey(mode_cycle_vk, [&plugin]() {
        plugin.CycleTrackingMode();
        HT_LOG("[hotkey] mode cycle -> %s", plugin.TrackingModeName());
    });
    m_poller.AddHotkey(kVkT, [&plugin]() {
        if (!CtrlShiftHeld()) return;
        plugin.Recenter();
        HT_LOG("[hotkey] recenter (chord)");
    });
    m_poller.AddHotkey(kVkY, [&plugin]() {
        if (!CtrlShiftHeld()) return;
        plugin.ToggleEnabled();
        HT_LOG("[hotkey] toggle (chord) -> %s", plugin.IsEnabled() ? "on" : "off");
    });
    m_poller.AddHotkey(kVkH, [&plugin]() {
        if (!CtrlShiftHeld()) return;
        plugin.ToggleYawMode();
        HT_LOG("[hotkey] yaw mode (chord) -> %s",
               plugin.IsWorldSpaceYaw() ? "world-space" : "camera-local");
    });
    m_poller.AddHotkey(kVkG, [&plugin]() {
        if (!CtrlShiftHeld()) return;
        plugin.CycleTrackingMode();
        HT_LOG("[hotkey] mode cycle (chord) -> %s", plugin.TrackingModeName());
    });
    m_poller.Start(16);
}

void HotkeyHandler::Stop() {
    m_poller.Stop();
}

}  // namespace headtracking
