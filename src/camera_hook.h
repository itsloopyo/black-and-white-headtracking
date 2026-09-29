#pragma once

namespace headtracking {

// Installs MinHook detours on the engine's camera-matrix builders and
// related render entry points so the head pose is injected into the
// render-side matrices only; game logic continues to read the clean
// camera struct. See camera_hook.cpp for the full hook map and rationale.
class CameraHook {
public:
    CameraHook() = default;
    ~CameraHook();

    bool Install();
    void Uninstall();
};

// Distance from the camera eye to its look-at target (engine world units),
// captured each frame from the camera builders. This is the zoom level: it
// shrinks when zooming in and grows when zooming out. Returns 0 before the
// first camera build. Used to scale positional tracking so the on-screen
// effect is constant across zoom.
float GetFocalDistance();

void SetVirtualCursor(bool active, int x, int y);

}  // namespace headtracking
