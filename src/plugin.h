#pragma once

#include <atomic>
#include <memory>
#include <optional>

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
    // Logs a save's lines, and its reason when it wrote nothing. Save never retries: the session
    // keeps the new value either way.
    void LogSave(const char* what, const cameraunlock::config::ConfigSaveResult& saved);

    // The session itself re-reads the receiver's connection locality each
    // update and points both processors at LocalSmoothing or RemoteSmoothing.
    // This only reports the switch, so a bug report can say which of the two
    // values was actually in effect.
    void LogConnectionLocality();

    // Built once in Initialize, before anything reads CameraUnlock.ini.
    std::optional<cameraunlock::config::ConfigOwner<Config>> m_owner;
    Config m_config;
    std::atomic<bool> m_enabled{false};

    bool m_remoteConnection = false;
    bool m_remoteConnectionKnown = false;
    std::atomic<bool> m_worldSpaceYaw{true};

    cameraunlock::UdpReceiver m_receiver;
    using Session = cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver>;
    // Without IsRemoteConnection() on the receiver the session silently falls
    // back to LocalSmoothing forever, with nothing at the call site to show it.
    static_assert(Session::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection() to select Local/RemoteSmoothing");
    Session m_session{m_receiver};
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
