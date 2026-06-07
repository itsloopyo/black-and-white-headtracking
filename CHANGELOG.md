# Changelog

## [0.1.1] - 2026-06-07

### Added

- add HeadTrackingSession and expand C++ core with RE Engine, Unreal, and tracking-session modules
- aim projection, reframework/unreal hooks, input/logging hardening, games
- add Mass Effect Legendary Edition to games catalog
- expand games catalog, fix unicode games.json read, stage launcher manifest
- add Pacific Drive to games catalog
- add Homeworld: Remastered Collection to games catalog
- add manifest-mode installer validator and ASI loader subdir support
- authenticate GitHub API requests via env token when present

### Fixed

- fail fast in ASI dev-deploy when the game is running
- restore il2cpp camera position by undoing applied local delta
- set SO_REUSEADDR so the receiver reclaims its port on relaunch

### Other

- templates: add uninstall.ps1; data: add Deus Ex Mankind Divided
- powershell: add NightlyRelease module for Patreon-gated nightly builds
- Add release nightly dispatch and publisher shim
- protocol: disable SIO_UDP_CONNRESET and add one-shot receiver diagnostics; powershell: write nightly manifest.json without UTF-8 BOM; data: add Mixtape
- powershell: stop redirecting git stderr in Update-CameraUnlockCoreToRemoteTip
- powershell: publish dev builds as GitHub pre-releases
- protocol: disable SIO_UDP_CONNRESET and add one-shot receiver diagnostics
- data: add Mixtape
- powershell: stop redirecting git stderr in Update-CameraUnlockCoreToRemoteTip
- powershell: run gh under Continue so its stderr doesn't abort the dev-release publish
- reframework: strip VR runtime DLLs on install for flatscreen mode
- reframework: cache GetValue method and avoid per-call heap in ArrayGetValue; data: add BioShock Infinite
- uninstall: remove reframework_revision.txt marker dropped at game root
- install: render MOD_CONTROLS multi-line via percent expansion
- Add YAPYAP to games.json
- powershell: write state file BOM-less so Lopari JSON parser accepts it

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
