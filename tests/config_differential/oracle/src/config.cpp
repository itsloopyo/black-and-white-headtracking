#include "config.h"

#include <Windows.h>
#include <cmath>
#include <filesystem>

#include "cameraunlock/config/ini_reader.h"
#include "debug_log.h"

namespace headtracking {

constexpr float kDefaultLocalSmoothing =
    static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
constexpr float kDefaultRemoteSmoothing =
    static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

// The old value is deliberately NOT migrated into the new keys. The single
// Smoothing value carried a hidden 0.15 floor, so the number in an existing
// config does not mean what it used to: copying it across would hand a local
// user smoothing they never chose under the new semantics, and copying it into
// only one of the two keys would be a guess about which connection they were on.
static void WarnRetiredSmoothingKey(const cameraunlock::IniReader& reader,
                                    const char* section, const char* key) {
    if (reader.ReadString(section, key, "").empty()) return;
    HT_LOG(
        "[config] key [%s] %s has been retired and is IGNORED. Smoothing is now two "
        "keys: LocalSmoothing (default 0, applies to a tracker on this machine) and "
        "RemoteSmoothing (default 0.15, applies to a tracker on the network). The "
        "old value is not migrated because the semantics changed - it carried a "
        "hidden 0.15 floor that no longer exists. Set the two new keys.",
        section, key);
}

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

    // Validation only, never a floor: a non-finite value cannot reach the
    // smoothing math, but whatever the user set inside the range stands, 0.0
    // included. `fallback` is the shipped default of the key being read, and
    // the two keys do not share one - a refused RemoteSmoothing has to land on
    // 0.15, not on LocalSmoothing's 0.0, or a phone on WiFi silently ends up
    // with no smoothing at all on raw network jitter.
    //
    // Out-of-range values are clamped rather than refused. That is not because
    // the math breaks: the core clamps its own interpolation speed to
    // [0.1, 50], so a smoothing above 1 no longer drives the per-frame factor
    // negative, it just saturates. The clamp is here so the value the mod acts
    // on and the value the INI advertises stay the same number.
    auto smoothing_or = [](float v, float fallback) {
        float f = std::isfinite(v) ? v : fallback;
        if (f < 0.0f) return 0.0f;
        return f > 0.99f ? 0.99f : f;
    };
    auto read_smoothing = [&](const char* key, float fallback) {
        const float raw = r.ReadFloat("Smoothing", key, fallback);
        const float clean = smoothing_or(raw, fallback);
        // A NaN raw value compares unequal to everything including itself, so
        // it takes this branch too and the substitution is never silent.
        if (raw != clean) {
            HT_LOG("[config] Smoothing.%s=%.4f is out of range or not finite; using %.4f",
                   key, raw, clean);
        }
        return clean;
    };
    c.local_smoothing  = read_smoothing("LocalSmoothing",  kDefaultLocalSmoothing);
    c.remote_smoothing = read_smoothing("RemoteSmoothing", kDefaultRemoteSmoothing);

    WarnRetiredSmoothingKey(r, "Smoothing", "Amount");
    WarnRetiredSmoothingKey(r, "Position", "Smoothing");

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
    constexpr cameraunlock::PositionSettings kPositionDefaults{};
    c.pos_limit_x      = limit_or(r.ReadFloat("Position", "LimitX",     kPositionDefaults.limit_x),
                                  kPositionDefaults.limit_x);
    c.pos_limit_y      = limit_or(r.ReadFloat("Position", "LimitY",     kPositionDefaults.limit_y),
                                  kPositionDefaults.limit_y);
    c.pos_limit_z      = limit_or(r.ReadFloat("Position", "LimitZ",     kPositionDefaults.limit_z),
                                  kPositionDefaults.limit_z);
    c.pos_limit_z_back = limit_or(r.ReadFloat("Position", "LimitZBack", kPositionDefaults.limit_z_back),
                                  kPositionDefaults.limit_z_back);

    c.toggle_vk    = r.ReadHex("Hotkeys", "Toggle",   0x23);
    c.yaw_mode_vk  = r.ReadHex("Hotkeys", "YawMode",  0x22);
    c.mode_cycle_vk = r.ReadHex("Hotkeys", "ModeCycle", 0x21);
    c.debounce_ms  = r.ReadInt("Hotkeys", "DebounceMs", 200);

    c.world_space_yaw = r.ReadBool("View", "WorldSpaceYaw", true);

    c.log_position_trace = r.ReadBool("Logging", "PositionTrace", false);

    return c;
}

}  // namespace headtracking
