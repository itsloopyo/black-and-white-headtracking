# Changelog

## [Unreleased]

### Changed

- The tracker owns the centre. The recenter hotkeys (`Home` / `Ctrl+Shift+T`)
  and their `[Hotkeys] Recenter` INI entry are gone, along with the mod-side
  centre capture; the tracker pose is applied as absolute. Centre the view in
  your tracker app instead.

- Smoothing is now two settings instead of one: `[Smoothing] LocalSmoothing`
  (default `0.0`) applies when the tracker runs on this PC, `[Smoothing]
  RemoteSmoothing` (default `0.15`) applies when it is a phone or other device
  on the network. Which one is used is decided per connection from the packet's
  source address and is re-evaluated when the source changes, so switching
  between a local OpenTrack instance and a phone takes effect without a restart.
- Removed `[Smoothing] Amount` and `[Position] Smoothing`. Both new values cover
  rotation and position alike, so there is no separate position smoothing
  setting.
- Removed the hidden 0.15 smoothing floor. It silently overrode whatever the
  user set, so a tracker on the same machine now gets zero-latency tracking by
  default.
- Removed `[Debug] LogToFile`. The mod always writes `HeadTracking_debug.log`
  now. It shipped off, so anyone reporting a problem had to be told to enable it
  and play again before there was anything to read - and flipping the default
  would not have reached the existing users who already have the key set to
  `false` in their INI. The log is truncated on every launch, so keeping it on
  costs a single file next to the EXE.
- The positional calibration trace stops after the first minute of tracked
  position instead of writing a line every second for the whole session
  (roughly 540 KB an hour, which buried the startup chain).
- Troubleshooting now names the log file, `HeadTracking_debug.log` next to
  `runblack.exe`, instead of saying "the log next to the game executable".

## [0.1.4] - 2026-08-03

### Fixed

- harden release.ps1 - changelog gate before version bump, add -Force
- drop pinned VS 2022 CMake generator, let CMake auto-detect

### Other

- Link Discord, Lopari and Headcam from the README

## [0.1.2] - 2026-06-07

### Added

- add HeadTrackingSession and expand C++ core with RE Engine, Unreal, and tracking-session modules
- aim projection, reframework/unreal hooks, input/logging hardening, games
- add Mass Effect Legendary Edition to games catalog
- expand games catalog, fix unicode games.json read, stage launcher manifest
- add Pacific Drive to games catalog
- add Homeworld: Remastered Collection to games catalog
- add manifest-mode installer validator and ASI loader subdir support
- authenticate GitHub API requests via env token when present
- add R.E.P.O. detection data

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
- powershell: stop redirecting git stderr in Invoke-VersionCommit

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
