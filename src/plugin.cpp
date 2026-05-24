#include "plugin.h"

#include <Windows.h>
#include <chrono>

#include "camera_hook.h"
#include "hotkey_handler.h"
#include "debug_log.h"

namespace headtracking {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

// First-call seed and out-of-range fallback for the processor's dt input.
// 1/60s is a reasonable assumption before any inter-frame interval is known.
constexpr float kFallbackDtSeconds = 1.0f / 60.0f;
// Discard implausible intervals (clock jumps, debugger pauses, first frame
// after a long stall) and feed the processor the seed instead.
constexpr float kMaxPlausibleDtSeconds = 1.0f;

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
    processor.SetSmoothing(c.smoothing);
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
    ps.limit_y = c.pos_limit_y;
    ps.limit_z = c.pos_limit_z;
    ps.limit_z_back = c.pos_limit_z_back;
    ps.smoothing = c.pos_smoothing;
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
    SetFileLogging(m_config.log_to_file);
    m_enabled.store(m_config.enabled_on_startup);
    m_worldSpaceYaw.store(m_config.world_space_yaw);
    m_trackingMode.store(static_cast<int>(
        m_config.pos_enabled ? TrackingMode::SixDof : TrackingMode::RotationOnly));

    ApplyRotationConfig(m_processor, m_config);
    ApplyPositionConfig(m_posProcessor, m_config);

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
    m_hotkeys->Start(*this, m_config.recenter_vk, m_config.toggle_vk, m_config.yaw_mode_vk,
                     m_config.mode_cycle_vk);
    HT_LOG("[plugin] initialized");
    return true;
}

void Plugin::Shutdown() {
    if (m_hotkeys) { m_hotkeys->Stop(); m_hotkeys.reset(); }
    if (m_cameraHook) { m_cameraHook->Uninstall(); m_cameraHook.reset(); }
    m_receiver.Stop();
}

void Plugin::Recenter() {
    m_recenterRequested.store(true);
}

void Plugin::ToggleYawMode() {
    const bool next = !m_worldSpaceYaw.load();
    m_worldSpaceYaw.store(next);
    HT_LOG("[plugin] yaw mode -> %s", next ? "world-space" : "camera-local");
}

void Plugin::CycleTrackingMode() {
    const int next = (m_trackingMode.load() + 1) % 3;
    m_trackingMode.store(next);
    HT_LOG("[plugin] tracking mode -> %s", TrackingModeName());
}

const char* Plugin::TrackingModeName() const {
    switch (GetTrackingMode()) {
        case TrackingMode::SixDof:       return "6DOF (rotation + position)";
        case TrackingMode::RotationOnly: return "rotation only";
        case TrackingMode::PositionOnly: return "position only";
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

    if (m_recenterRequested.exchange(false)) {
        m_receiver.Recenter();
        m_processor.Reset();
        // Recentre position too: the receiver stores no position offset, so the
        // current raw head position becomes the processor's neutral point.
        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        if (m_receiver.GetPosition(cx, cy, cz)) {
            m_posProcessor.SetCenter(cameraunlock::PositionData(cx, cy, cz));
        }
        m_posProcessor.ResetSmoothing();
    }

    float ry = 0.0f, rp = 0.0f, rr = 0.0f;
    if (!m_receiver.GetRotation(ry, rp, rr)) return false;

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const int64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    float dt = m_lastPollTimeUs == 0
                 ? kFallbackDtSeconds
                 : (now_us - m_lastPollTimeUs) / 1e6f;
    m_lastPollTimeUs = now_us;
    if (dt < 0.0f || dt > kMaxPlausibleDtSeconds) dt = kFallbackDtSeconds;

    // Always process so the smoothed rotation state stays warm across mode
    // switches; the mode only decides what we hand to the camera.
    auto pose = m_processor.Process(ry, rp, rr, dt);
    const TrackingMode mode = GetTrackingMode();
    const bool rot_active = (mode != TrackingMode::PositionOnly);
    const bool pos_active = (mode != TrackingMode::RotationOnly);

    if (rot_active) {
        yaw   = pose.yaw   * kDegToRad;
        pitch = pose.pitch * kDegToRad;
        roll  = pose.roll  * kDegToRad;
    } else {
        yaw = pitch = roll = 0.0f;
    }
    m_cachedYaw.store(yaw,   std::memory_order_release);
    m_cachedPitch.store(pitch, std::memory_order_release);
    m_cachedRoll.store(roll,  std::memory_order_release);
    m_cachedValid.store(true, std::memory_order_release);

    // Positional tracking. Run the raw head position (mm -> m) through the
    // shared position pipeline (recentre, pivot-compensate, sensitivity,
    // smooth, clamp), using the smoothed pre-sensitivity rotation so the
    // pivot-compensation matches the orientation actually shown. The result
    // is a camera-local displacement in metres; scale it to engine units.
    // The camera hook applies it as the final shift on the render matrix.
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    if (pos_active && m_receiver.GetPosition(px, py, pz)) {
        float syaw = 0.0f, spitch = 0.0f, sroll = 0.0f;
        m_processor.GetSmoothedRotation(syaw, spitch, sroll);
        const auto rotq = cameraunlock::math::Quat4::FromYawPitchRoll(syaw, spitch, sroll);
        const cameraunlock::PositionData raw(px, py, pz);  // OpenTrack sends metres
        const cameraunlock::math::Vec3 off = m_posProcessor.Process(raw, rotq, dt);

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
        const float wx = off.x * scale;
        const float wy = off.y * scale;
        const float wz = off.z * scale;
        m_cachedPosX.store(wx, std::memory_order_release);
        m_cachedPosY.store(wy, std::memory_order_release);
        m_cachedPosZ.store(wz, std::memory_order_release);
        m_cachedPosValid.store(true, std::memory_order_release);

        // Throttled (~1/s) calibration trace: raw tracker mm, clamped metres,
        // and the engine-unit offset actually handed to the camera hook. Use
        // this to size WorldScale and confirm each axis moves the right way.
        static int64_t s_lastPosLogUs = 0;
        if (now_us - s_lastPosLogUs > 1'000'000) {
            s_lastPosLogUs = now_us;
            HT_LOG("[pos] raw_m=(%.3f,%.3f,%.3f) clamped_m=(%.3f,%.3f,%.3f) "
                   "world=(%.2f,%.2f,%.2f) scale=%.1f focal=%.1f zoom=%.2f",
                   px, py, pz, off.x, off.y, off.z, wx, wy, wz,
                   m_config.pos_world_scale, focal, zoom);
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
