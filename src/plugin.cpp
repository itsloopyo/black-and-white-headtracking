#include "plugin.h"

#include "camera_hook.h"
#include "hotkey_handler.h"
#include "debug_log.h"

#include "cameraunlock/math/smoothing_utils.h"

namespace headtracking {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

void ApplyRotationConfig(cameraunlock::TrackingProcessor& processor, const Config& c) {
    cameraunlock::SensitivitySettings s;
    s.yaw = c.sens_yaw;
    s.pitch = c.sens_pitch;
    s.roll = c.sens_roll;
    s.invert_yaw = c.invert_yaw;
    s.invert_pitch = c.invert_pitch;
    s.invert_roll = c.invert_roll;
    processor.SetSensitivity(s);

    cameraunlock::DeadzoneSettings d;
    d.yaw = c.deadzone_yaw;
    d.pitch = c.deadzone_pitch;
    d.roll = c.deadzone_roll;
    processor.SetDeadzone(d);
}

void ApplyPositionConfig(cameraunlock::PositionProcessor& processor, const Config& c) {
    cameraunlock::PositionSettings ps;
    ps.sensitivity_x = c.pos_sens_x;
    ps.sensitivity_y = c.pos_sens_y;
    ps.sensitivity_z = c.pos_sens_z;
    ps.invert_x = c.pos_invert_x;
    ps.invert_y = c.pos_invert_y;
    ps.invert_z = c.pos_invert_z;
    ps.limit_x = c.pos_limit_x;
    // The clamp is [-limit_y_down, +limit_y] and limit_y_down carries its own
    // default, so mirror the one configured vertical limit the way
    // PositionSettings::Symmetric does. Left unset, raising LimitY widened the
    // upward budget only and downward travel stayed pinned at 0.20m.
    ps.limit_y = c.pos_limit_y;
    ps.limit_y_down = c.pos_limit_y;
    ps.limit_z = c.pos_limit_z;
    ps.limit_z_back = c.pos_limit_z_back;
    // Position has no smoothing setting of its own - it rides the same two
    // values as rotation, pushed through the session after these settings land.
    ps.local_smoothing = c.local_smoothing;
    ps.remote_smoothing = c.remote_smoothing;
    processor.SetSettings(ps);
    // The core default (0.15) synthesises translation from head rotation to
    // cancel a webcam pivot in front of the face. Our trackers report position
    // directly, so that term only injects phantom rotation-coupled movement
    // (which drags world-anchored UI off its targets). Disable it.
    processor.SetTrackerPivotForward(0.0f);
}
}  // namespace

Plugin& GetPlugin() {
    static Plugin instance;
    return instance;
}

Plugin::Plugin() = default;
Plugin::~Plugin() { Shutdown(); }

bool Plugin::Initialize() {
    m_config = Config::LoadOrCreateDefault();
    m_enabled.store(m_config.enabled_on_startup);
    m_worldSpaceYaw.store(m_config.world_space_yaw);
    m_session.SetMode(m_config.pos_enabled
                          ? cameraunlock::TrackingMode::RotationAndPosition
                          : cameraunlock::TrackingMode::RotationOnly);

    ApplyRotationConfig(m_session.GetProcessor(), m_config);
    ApplyPositionConfig(m_session.GetPositionProcessor(), m_config);
    // One pair of values for rotation and position alike. The session picks
    // between them per connection from the receiver's source-address check, so
    // nothing here decides which one is in effect.
    m_session.SetLocalSmoothing(m_config.local_smoothing);
    m_session.SetRemoteSmoothing(m_config.remote_smoothing);

    m_receiver.SetLog([](const std::string& msg) {
        HT_LOG("[receiver] %s", msg.c_str());
    });
    if (m_receiver.Start(m_config.port)) {
        HT_LOG("[plugin] listening on UDP %u", m_config.port);
    } else {
        HT_LOG("[plugin] UDP port %u busy, receiver will retry in background", m_config.port);
    }

    m_cameraHook = std::make_unique<CameraHook>();
    if (!m_cameraHook->Install()) {
        HT_LOG("[plugin] camera hook installation failed");
        return false;
    }

    m_hotkeys = std::make_unique<HotkeyHandler>();
    m_hotkeys->Start(*this, m_config.toggle_vk, m_config.yaw_mode_vk, m_config.mode_cycle_vk);
    // The settings actually in effect, once. A bug report arrives as this log
    // and nothing else, and every "it feels wrong" question (too strong, too
    // laggy, wrong axis, hotkey does nothing) is answered by these numbers.
    HT_LOG("[plugin] initialized - enabled=%d mode=%s yaw=%s "
           "smoothing local=%.2f remote=%.2f | rot sens=(%.2f,%.2f,%.2f) "
           "invert=(%d,%d,%d) | pos scale=%.1f zoomRef=%.1f zoomMax=%.2f "
           "sens=(%.2f,%.2f,%.2f) invert=(%d,%d,%d) | hotkeys "
           "toggle=0x%02X yawMode=0x%02X modeCycle=0x%02X",
           m_enabled.load() ? 1 : 0, TrackingModeName(),
           m_worldSpaceYaw.load() ? "world-space" : "camera-local",
           m_config.local_smoothing, m_config.remote_smoothing,
           m_config.sens_yaw, m_config.sens_pitch, m_config.sens_roll,
           m_config.invert_yaw ? 1 : 0, m_config.invert_pitch ? 1 : 0,
           m_config.invert_roll ? 1 : 0,
           m_config.pos_world_scale, m_config.pos_zoom_reference,
           m_config.pos_zoom_scale_max,
           m_config.pos_sens_x, m_config.pos_sens_y, m_config.pos_sens_z,
           m_config.pos_invert_x ? 1 : 0, m_config.pos_invert_y ? 1 : 0,
           m_config.pos_invert_z ? 1 : 0,
           m_config.toggle_vk, m_config.yaw_mode_vk, m_config.mode_cycle_vk);
    return true;
}

void Plugin::Shutdown() {
    if (m_hotkeys) { m_hotkeys->Stop(); m_hotkeys.reset(); }
    if (m_cameraHook) { m_cameraHook->Uninstall(); m_cameraHook.reset(); }
    m_receiver.Stop();
}

// The session re-reads the receiver's source-address check every update, so a
// player who switches from a local OpenTrack instance to a phone on WiFi
// mid-session gets the other smoothing parameter without restarting the game.
// This only records the switch.
void Plugin::LogConnectionLocality() {
    const bool isRemote = m_session.IsRemoteConnection();
    if (m_remoteConnectionKnown && isRemote == m_remoteConnection) return;

    m_remoteConnection = isRemote;
    m_remoteConnectionKnown = true;
    HT_LOG("[plugin] tracker source is %s - smoothing=%.2f",
           isRemote ? "a remote device" : "on this machine",
           cameraunlock::math::GetEffectiveSmoothing(
               m_config.local_smoothing, m_config.remote_smoothing, isRemote));
}

void Plugin::ToggleYawMode() {
    const bool next = !m_worldSpaceYaw.load();
    m_worldSpaceYaw.store(next);
    HT_LOG("[plugin] yaw mode -> %s", next ? "world-space" : "camera-local");
}

void Plugin::CycleTrackingMode() {
    m_session.CycleMode();
    HT_LOG("[plugin] tracking mode -> %s", TrackingModeName());
}

const char* Plugin::TrackingModeName() const {
    switch (m_session.GetMode()) {
        case cameraunlock::TrackingMode::RotationAndPosition: return "6DOF (rotation + position)";
        case cameraunlock::TrackingMode::RotationOnly:        return "rotation only";
        case cameraunlock::TrackingMode::PositionOnly:        return "position only";
    }
    return "?";
}

bool Plugin::GetCurrentRotationRadians(float& yaw, float& pitch, float& roll) {
    if (!m_enabled.load()) return false;

    static bool s_loggedConnected = false;
    if (!m_receiver.IsReceiving()) {
        if (s_loggedConnected) {
            HT_LOG("[plugin] tracking source disconnected (no packets within timeout)");
            s_loggedConnected = false;
        }
        return false;
    }
    if (!s_loggedConnected) {
        HT_LOG("[plugin] tracking source connected (remote=%d)",
               m_receiver.IsRemoteConnection() ? 1 : 0);
        s_loggedConnected = true;
    }

    const float dt = m_frameClock.Tick();
    if (!m_session.Update(dt)) return false;
    LogConnectionLocality();

    float yaw_deg = 0.0f, pitch_deg = 0.0f, roll_deg = 0.0f;
    m_session.GetRotation(yaw_deg, pitch_deg, roll_deg);
    yaw   = yaw_deg   * kDegToRad;
    pitch = pitch_deg * kDegToRad;
    roll  = roll_deg  * kDegToRad;
    m_cachedYaw.store(yaw,   std::memory_order_release);
    m_cachedPitch.store(pitch, std::memory_order_release);
    m_cachedRoll.store(roll,  std::memory_order_release);
    m_cachedValid.store(true, std::memory_order_release);

    // Positional tracking. The session has run the raw head position through
    // the shared pipeline (sensitivity, smooth, clamp); the result
    // is a camera-local displacement in metres. Scale it to engine units.
    // The camera hook applies it as the final shift on the render matrix.
    float ox = 0.0f, oy = 0.0f, oz = 0.0f;
    if (m_session.GetPositionOffset(ox, oy, oz)) {
        // Scale by zoom so the on-screen effect is constant: a camera move
        // produces screen parallax inversely with the distance to what you're
        // looking at, so the world offset must scale with that focal distance.
        float zoom = 1.0f;
        const float focal = GetFocalDistance();
        if (focal > 0.0f) {
            float ref = m_config.pos_zoom_reference;
            if (ref <= 0.0f) {
                if (m_zoomRef <= 0.0f) m_zoomRef = focal;  // lock to first gameplay zoom
                ref = m_zoomRef;
            }
            if (ref > 0.0f) {
                zoom = focal / ref;
                // B&W's focal distance spans ~500x; uncapped this lunges the
                // camera at full zoom-out. Clamp to [1/max, max].
                const float hi = m_config.pos_zoom_scale_max;
                const float lo = 1.0f / hi;
                if (zoom > hi) zoom = hi;
                if (zoom < lo) zoom = lo;
            }
        }
        const float scale = m_config.pos_world_scale * zoom;
        const float wx = ox * scale;
        const float wy = oy * scale;
        const float wz = oz * scale;
        m_cachedPosX.store(wx, std::memory_order_release);
        m_cachedPosY.store(wy, std::memory_order_release);
        m_cachedPosZ.store(wz, std::memory_order_release);
        m_cachedPosValid.store(true, std::memory_order_release);

        // Calibration trace: raw tracker metres, clamped metres, and the
        // engine-unit offset actually handed to the camera hook. Use this to
        // size WorldScale and confirm each axis moves the right way.
        //
        // Off unless [Logging] PositionTrace is set, and then one line a second
        // for the first minute of tracked position. Unbounded it was ~540 KB an
        // hour, which buries the startup chain a bug report is read for; a
        // minute is long enough to lean in each direction and read the numbers
        // back.
        static float s_posLogAccum = 0.0f;
        static int s_posLogLines = 0;
        if (m_config.log_position_trace && s_posLogLines < 60) {
            s_posLogAccum += dt;
            if (s_posLogAccum >= 1.0f) {
                s_posLogAccum = 0.0f;
                ++s_posLogLines;
                float rx = 0.0f, ry = 0.0f, rz = 0.0f;
                m_receiver.GetPosition(rx, ry, rz);
                HT_LOG("[pos] raw_m=(%.3f,%.3f,%.3f) clamped_m=(%.3f,%.3f,%.3f) "
                       "world=(%.2f,%.2f,%.2f) scale=%.1f focal=%.1f zoom=%.2f",
                       rx, ry, rz, ox, oy, oz, wx, wy, wz,
                       m_config.pos_world_scale, focal, zoom);
            }
        }
    } else {
        m_cachedPosValid.store(false, std::memory_order_release);
    }
    return true;
}

bool Plugin::GetCachedRotationRadians(float& yaw, float& pitch, float& roll) const {
    if (!m_cachedValid.load(std::memory_order_acquire)) return false;
    yaw   = m_cachedYaw.load(std::memory_order_acquire);
    pitch = m_cachedPitch.load(std::memory_order_acquire);
    roll  = m_cachedRoll.load(std::memory_order_acquire);
    return true;
}

bool Plugin::GetCurrentPositionOffset(float& x, float& y, float& z) const {
    if (!m_cachedPosValid.load(std::memory_order_acquire)) return false;
    x = m_cachedPosX.load(std::memory_order_acquire);
    y = m_cachedPosY.load(std::memory_order_acquire);
    z = m_cachedPosZ.load(std::memory_order_acquire);
    return true;
}

}  // namespace headtracking
