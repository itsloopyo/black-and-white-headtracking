# Changelog

## [0.1.0] - 2026-05-17

### Added
- Initial head tracking support for Black & White (2001) via the OpenTrack UDP
  protocol.
- 32-bit DLL injected into `runblack.exe` by `bw-headtracking-launcher.exe`.
- DirectX 7 view-matrix interception: hooks `ddraw!DirectDrawCreateEx`, walks
  the `IDirectDraw7 -> IDirect3D7 -> IDirect3DDevice7` interface chain, and
  vtable-patches `IDirect3DDevice7::SetTransform`. When the engine commits a
  view matrix we post-multiply it with the tracked yaw/pitch/roll rotation
  around the camera origin recovered from `inverse(view)`.
- INI config (`HeadTracking.ini`) for port, sensitivity, smoothing, deadzone,
  hotkeys.
- Hotkeys: Home = recenter, End = toggle.
- Built on top of [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core).
