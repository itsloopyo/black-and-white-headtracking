#include "config.h"

#include <Windows.h>
#include <cmath>
#include <filesystem>

#include "cameraunlock/config/ini_reader.h"
#include "debug_log.h"

namespace headtracking {

std::string Config::IniPath() {
    char buf[MAX_PATH] = {};
    // GetModuleFileNameA does not guarantee null-termination on truncation, so
    // bound the path by the returned length instead of reading the raw buffer.
    DWORD len = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    if (len == 0 || len >= sizeof(buf)) return "HeadTracking.ini";
    std::filesystem::path p(std::string(buf, len));
    return (p.parent_path() / "HeadTracking.ini").string();
}

void Config::WriteDefault(const std::string& path) {
    cameraunlock::IniWriter w;
    if (!w.Open(path)) {
        HT_LOG("[config] failed to write default ini at %s", path.c_str());
        return;
    }
    w.WriteComment(" black and white head tracking - default config");
    w.WriteBlankLine();
    w.WriteSection("Network");
    w.WriteInt("Port", 4242);
    w.WriteBool("EnableOnStartup", true);
    w.WriteBlankLine();
    w.WriteSection("Sensitivity");
    w.WriteDouble("Yaw", 1.0);
    w.WriteDouble("Pitch", 1.0);
    w.WriteDouble("Roll", 1.0);
    w.WriteBool("InvertYaw", false);
    w.WriteBool("InvertPitch", false);
    w.WriteBool("InvertRoll", false);
    w.WriteBlankLine();
    w.WriteSection("Smoothing");
    w.WriteDouble("Amount", 0.0);
    w.WriteBlankLine();
    w.WriteSection("Deadzone");
    w.WriteDouble("Yaw", 0.0);
    w.WriteDouble("Pitch", 0.0);
    w.WriteDouble("Roll", 0.0);
    w.WriteBlankLine();
    w.WriteSection("Position");
    w.WriteComment(" 6DOF head position, applied camera-local as the final camera shift");
    w.WriteBool("Enabled", true);
    w.WriteComment(" WorldScale = engine units per metre of head movement; main tuning knob");
    w.WriteDouble("WorldScale", 40.0);
    w.WriteComment(" ZoomReference = focal distance WorldScale is tuned at; 0 = auto-lock first zoom");
    w.WriteDouble("ZoomReference", 0.0);
    w.WriteComment(" ZoomScaleMax = clamp on zoom scaling [1/max, max]; lower if zoom-out too strong");
    w.WriteDouble("ZoomScaleMax", 2.5);
    w.WriteDouble("SensX", 1.0);
    w.WriteDouble("SensY", 1.0);
    w.WriteDouble("SensZ", 1.0);
    w.WriteBool("InvertX", false);
    w.WriteBool("InvertY", false);
    w.WriteBool("InvertZ", false);
    w.WriteComment(" Movement envelope in metres before world scaling");
    w.WriteDouble("LimitX", 0.30);
    w.WriteDouble("LimitY", 0.20);
    w.WriteDouble("LimitZ", 0.40);
    w.WriteDouble("LimitZBack", 0.10);
    w.WriteDouble("Smoothing", 0.15);
    w.WriteBlankLine();
    w.WriteSection("Hotkeys");
    w.WriteHex("Recenter", 0x24);
    w.WriteHex("Toggle", 0x23);
    w.WriteHex("YawMode", 0x22);
    w.WriteComment(" Page Up: cycle 6DOF -> rotation-only -> position-only");
    w.WriteHex("ModeCycle", 0x21);
    w.WriteInt("DebounceMs", 200);
    w.WriteBlankLine();
    w.WriteSection("View");
    w.WriteComment(" true = horizon-locked yaw (default), false = camera-local yaw");
    w.WriteBool("WorldSpaceYaw", true);
    w.WriteBlankLine();
    w.WriteSection("Debug");
    w.WriteBool("LogToFile", false);
}

Config Config::LoadOrCreateDefault() {
    const std::string path = IniPath();
    if (!std::filesystem::exists(path)) {
        WriteDefault(path);
    }

    cameraunlock::IniReader r;
    Config c;
    if (!r.Open(path)) {
        HT_LOG("[config] could not open %s, using defaults", path.c_str());
        return c;
    }

    int port = r.ReadInt("Network", "Port", 4242);
    if (port < 1 || port > 65535) port = 4242;
    c.port = static_cast<uint16_t>(port);
    c.enabled_on_startup = r.ReadBool("Network", "EnableOnStartup", true);

    // A malformed INI value (e.g. NaN/Inf, or a negative deadzone) would feed
    // straight into the processor and the view-matrix math. Sensitivity is a
    // multiplier - a non-finite value poisons the output, so fall back to 1.0.
    // Deadzone is an angular threshold in degrees; negatives are meaningless.
    auto finite_or = [](float v, float fallback) {
        return std::isfinite(v) ? v : fallback;
    };
    c.sens_yaw   = finite_or(r.ReadFloat("Sensitivity", "Yaw",   1.0f), 1.0f);
    c.sens_pitch = finite_or(r.ReadFloat("Sensitivity", "Pitch", 1.0f), 1.0f);
    c.sens_roll  = finite_or(r.ReadFloat("Sensitivity", "Roll",  1.0f), 1.0f);
    c.invert_yaw   = r.ReadBool("Sensitivity", "InvertYaw",   false);
    c.invert_pitch = r.ReadBool("Sensitivity", "InvertPitch", false);
    c.invert_roll  = r.ReadBool("Sensitivity", "InvertRoll",  false);

    c.smoothing = r.ReadFloat("Smoothing", "Amount", 0.0f);
    if (!std::isfinite(c.smoothing) || c.smoothing < 0.0f) c.smoothing = 0.0f;
    if (c.smoothing > 0.99f) c.smoothing = 0.99f;

    auto deadzone_or = [](float v) {
        return (std::isfinite(v) && v > 0.0f) ? v : 0.0f;
    };
    c.deadzone_yaw   = deadzone_or(r.ReadFloat("Deadzone", "Yaw",   0.0f));
    c.deadzone_pitch = deadzone_or(r.ReadFloat("Deadzone", "Pitch", 0.0f));
    c.deadzone_roll  = deadzone_or(r.ReadFloat("Deadzone", "Roll",  0.0f));

    // Position. Sensitivity and world scale feed the view-matrix translation,
    // so a non-finite value would poison the camera; fall back to sane values.
    // Limits are a clamp envelope in metres - negatives are meaningless.
    c.pos_enabled  = r.ReadBool("Position", "Enabled", true);
    c.pos_world_scale = finite_or(r.ReadFloat("Position", "WorldScale", 40.0f), 40.0f);
    if (c.pos_world_scale < 0.0f) c.pos_world_scale = 0.0f;
    c.pos_zoom_reference = finite_or(r.ReadFloat("Position", "ZoomReference", 0.0f), 0.0f);
    if (c.pos_zoom_reference < 0.0f) c.pos_zoom_reference = 0.0f;
    c.pos_zoom_scale_max = finite_or(r.ReadFloat("Position", "ZoomScaleMax", 2.5f), 2.5f);
    if (c.pos_zoom_scale_max < 1.0f) c.pos_zoom_scale_max = 1.0f;
    c.pos_sens_x = finite_or(r.ReadFloat("Position", "SensX", 1.0f), 1.0f);
    c.pos_sens_y = finite_or(r.ReadFloat("Position", "SensY", 1.0f), 1.0f);
    c.pos_sens_z = finite_or(r.ReadFloat("Position", "SensZ", 1.0f), 1.0f);
    c.pos_invert_x = r.ReadBool("Position", "InvertX", false);
    c.pos_invert_y = r.ReadBool("Position", "InvertY", false);
    c.pos_invert_z = r.ReadBool("Position", "InvertZ", false);
    auto limit_or = [](float v, float fallback) {
        return (std::isfinite(v) && v >= 0.0f) ? v : fallback;
    };
    c.pos_limit_x      = limit_or(r.ReadFloat("Position", "LimitX",     0.30f), 0.30f);
    c.pos_limit_y      = limit_or(r.ReadFloat("Position", "LimitY",     0.20f), 0.20f);
    c.pos_limit_z      = limit_or(r.ReadFloat("Position", "LimitZ",     0.40f), 0.40f);
    c.pos_limit_z_back = limit_or(r.ReadFloat("Position", "LimitZBack", 0.10f), 0.10f);
    c.pos_smoothing = r.ReadFloat("Position", "Smoothing", 0.15f);
    if (!std::isfinite(c.pos_smoothing) || c.pos_smoothing < 0.0f) c.pos_smoothing = 0.0f;
    if (c.pos_smoothing > 0.99f) c.pos_smoothing = 0.99f;

    c.recenter_vk  = r.ReadHex("Hotkeys", "Recenter", 0x24);
    c.toggle_vk    = r.ReadHex("Hotkeys", "Toggle",   0x23);
    c.yaw_mode_vk  = r.ReadHex("Hotkeys", "YawMode",  0x22);
    c.mode_cycle_vk = r.ReadHex("Hotkeys", "ModeCycle", 0x21);
    c.debounce_ms  = r.ReadInt("Hotkeys", "DebounceMs", 200);

    c.world_space_yaw = r.ReadBool("View", "WorldSpaceYaw", true);

    c.log_to_file = r.ReadBool("Debug", "LogToFile", false);

    return c;
}

}  // namespace headtracking
