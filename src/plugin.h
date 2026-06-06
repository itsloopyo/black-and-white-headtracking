#pragma once

#include <atomic>
#include <memory>

#include "config.h"
#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/time/frame_clock.h"
#include "cameraunlock/tracking/head_tracking_session.h"

namespace headtracking {

class CameraHook;
class HotkeyHandler;

class Plugin {
public:
    Plugin();
    ~Plugin();

    bool Initialize();
    void Shutdown();

    bool IsEnabled() const { return m_enabled.load(); }
    void ToggleEnabled() { m_enabled.store(!m_enabled.load()); }
    void SetEnabled(bool e) { m_enabled.store(e); }

    bool IsWorldSpaceYaw() const { return m_worldSpaceYaw.load(); }
    void ToggleYawMode();

    void CycleTrackingMode();
    const char* TrackingModeName() const;

    void Recenter();

    // Pulls the latest UDP packet, runs it through the processor and returns
    // the processed yaw/pitch/roll in radians. Returns false if tracking is
    // disabled or no fresh data has arrived.
    bool GetCurrentRotationRadians(float& yaw, float& pitch, float& roll);

    // Thread-safe read of the most recent rotation produced by
    // GetCurrentRotationRadians. Does not poll the receiver or touch the
    // processor; safe to call from any thread (e.g. cursor compensation).
    bool GetCachedRotationRadians(float& yaw, float& pitch, float& roll) const;

    // Camera-local head displacement in engine world units, in view axes
    // (x=right, y=up, z=forward). Updated by GetCurrentRotationRadians; the
    // camera hook adds this to the render matrix as the final camera shift.
    // Returns false when positional tracking is off or no fresh sample exists.
    bool GetCurrentPositionOffset(float& x, float& y, float& z) const;

    const Config& GetConfig() const { return m_config; }

private:
    Config m_config;
    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_worldSpaceYaw{true};

    cameraunlock::UdpReceiver m_receiver;
    cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver> m_session{m_receiver};
    cameraunlock::time::FrameClock m_frameClock;

    std::unique_ptr<CameraHook>    m_cameraHook;
    std::unique_ptr<HotkeyHandler> m_hotkeys;

    // Auto-locked zoom reference (focal distance) when pos_zoom_reference is 0.
    float m_zoomRef = 0.0f;

    // Cached most-recent rotation, updated at the end of each successful
    // GetCurrentRotationRadians call. Read by GetCachedRotationRadians.
    std::atomic<float> m_cachedYaw{0.0f};
    std::atomic<float> m_cachedPitch{0.0f};
    std::atomic<float> m_cachedRoll{0.0f};
    std::atomic<bool>  m_cachedValid{false};

    // Cached most-recent camera-local position offset (engine world units),
    // updated alongside the rotation cache. Read by GetCurrentPositionOffset.
    std::atomic<float> m_cachedPosX{0.0f};
    std::atomic<float> m_cachedPosY{0.0f};
    std::atomic<float> m_cachedPosZ{0.0f};
    std::atomic<bool>  m_cachedPosValid{false};
};

Plugin& GetPlugin();

}  // namespace headtracking
