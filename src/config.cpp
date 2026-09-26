#include "config.h"

#include <Windows.h>
#include <filesystem>

#include "cameraunlock/config/ini_reader.h"
#include "debug_log.h"
#include "legacy_config/legacy_config.h"

namespace headtracking {

constexpr float kDefaultLocalSmoothing =
    static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
constexpr float kDefaultRemoteSmoothing =
    static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

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
    w.WriteComment(" Covers rotation and position alike. Which value is used is picked per");
    w.WriteComment(" connection from where the tracker sends from: LocalSmoothing for a");
    w.WriteComment(" tracker on this PC, RemoteSmoothing for a device on the network (a");
    w.WriteComment(" phone over WiFi, say). 0.0 instant, up to 0.99 max. Nothing floors");
    w.WriteComment(" either, so 0.0 really is zero-latency tracking.");
    w.WriteDouble("LocalSmoothing", kDefaultLocalSmoothing);
    w.WriteDouble("RemoteSmoothing", kDefaultRemoteSmoothing);
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
    w.WriteDouble("LimitX", cameraunlock::PositionSettings{}.limit_x);
    w.WriteDouble("LimitY", cameraunlock::PositionSettings{}.limit_y);
    w.WriteDouble("LimitZ", cameraunlock::PositionSettings{}.limit_z);
    w.WriteDouble("LimitZBack", cameraunlock::PositionSettings{}.limit_z_back);
    w.WriteBlankLine();
    w.WriteSection("Hotkeys");
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
    w.WriteSection("Logging");
    w.WriteComment(" One line a second for the first minute of tracked position, recording");
    w.WriteComment(" head metres, clamped metres, engine offset and focal distance. Turn on");
    w.WriteComment(" to tune WorldScale, or to read a focal value for ZoomReference.");
    w.WriteBool("PositionTrace", false);
}

Config Config::LoadOrCreateDefault() {
    const std::string path = IniPath();
    if (!std::filesystem::exists(path)) {
        WriteDefault(path);
    }

    legacy::Config l;
    Config c;
    if (legacy::Read(path.c_str(), l) == legacy::ReadStatus::Absent) {
        HT_LOG("[config] could not open %s, using defaults", path.c_str());
        return c;
    }

    c.port = l.port;
    c.enabled_on_startup = l.enabled_on_startup;
    c.sens_yaw = l.sens_yaw;
    c.sens_pitch = l.sens_pitch;
    c.sens_roll = l.sens_roll;
    c.invert_yaw = l.invert_yaw;
    c.invert_pitch = l.invert_pitch;
    c.invert_roll = l.invert_roll;
    c.local_smoothing = l.local_smoothing;
    c.remote_smoothing = l.remote_smoothing;
    c.deadzone_yaw = l.deadzone_yaw;
    c.deadzone_pitch = l.deadzone_pitch;
    c.deadzone_roll = l.deadzone_roll;
    c.pos_enabled = l.pos_enabled;
    c.pos_sens_x = l.pos_sens_x;
    c.pos_sens_y = l.pos_sens_y;
    c.pos_sens_z = l.pos_sens_z;
    c.pos_invert_x = l.pos_invert_x;
    c.pos_invert_y = l.pos_invert_y;
    c.pos_invert_z = l.pos_invert_z;
    c.pos_limit_x = l.pos_limit_x;
    c.pos_limit_y = l.pos_limit_y;
    c.pos_limit_z = l.pos_limit_z;
    c.pos_limit_z_back = l.pos_limit_z_back;
    c.pos_world_scale = l.pos_world_scale;
    c.pos_zoom_reference = l.pos_zoom_reference;
    c.pos_zoom_scale_max = l.pos_zoom_scale_max;
    c.toggle_vk = l.toggle_vk;
    c.yaw_mode_vk = l.yaw_mode_vk;
    c.mode_cycle_vk = l.mode_cycle_vk;
    c.debounce_ms = l.debounce_ms;
    c.world_space_yaw = l.world_space_yaw;
    c.log_position_trace = l.log_position_trace;
    return c;
}

}  // namespace headtracking
