# Changelog

## [Unreleased]

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Changed

- Settings move to `CameraUnlock.ini`, next to `runblack.exe`. Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when you never changed it from the default of the earlier version that wrote `HeadTracking.ini`, as far as the file shows which version that was, because `HeadTracking.ini` does not hold it or holds that default. It then follows `Defaults.ini`, so it takes the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none, which can differ from the default earlier versions used. A setting you changed is written with the value imported for it, or as `default` where that value equals its default at that start.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- A number in `HeadTracking.ini` that is not a number the mod can use (`nan`, `inf`) is written as `default` where the defaults the README shows set that setting to `default`, and as the built-in value elsewhere.
- A number in `HeadTracking.ini` outside the range a setting takes is brought to the nearest end of that range, and the log says so: a position limit above 10 is written as 10.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
  - A hotkey set to Ctrl, Shift or Alt on its own. That key goes down before the key of any chord made with it, so the hotkey is left unbound, and it keeps its Ctrl+Shift chord where it has one.
  - A `ZoomReference` or `ZoomScaleMax` you changed from its default. They were part of how leaning converts to the game's units, which the mod now does itself.
- An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. The import carries over the key you had bound to each action, and the chords, which earlier versions fixed in code, can now be changed or removed like any other key.
- A hotkey bound to a plain key no longer fires while Ctrl and Shift are both held, so Ctrl+Shift with that key reaches only a binding that names the chord.
- Settings are renamed in `CameraUnlock.ini`: `[Network] Port` is `UdpPort`; `[Network] EnableOnStartup` and `[View] WorldSpaceYaw` move to `[General]`; the `[Position]` limits are `PositionLimitX`, `PositionLimitY`, `PositionLimitYDown`, `PositionLimitZ` and `PositionLimitZBack`; and `[Hotkeys] Toggle`, `ModeCycle` and `YawMode` are `ToggleKey`, `CycleTrackingModeKey` and `YawModeKey`. `LimitY` bounded both directions, so it becomes both `PositionLimitY` and `PositionLimitYDown`, which can now be set apart. `[Position] Enabled` chose the tracking mode at startup; that is now the pair `RotationEnabled` and `PositionEnabled`. `[Logging] PositionTrace` keeps its name. The import carries every one of these values over.
- The tracking mode that Page Up or Ctrl+Shift+G selects, and the yaw mode that Page Down or Ctrl+Shift+H selects, are now saved to `CameraUnlock.ini` as soon as you change them and come back at the next start. End still changes the current session only.
- `uninstall.cmd` keeps `CameraUnlock.ini` and `HeadTracking.ini`, so your settings survive a reinstall. Earlier versions deleted `HeadTracking.ini` on uninstall.
- The installer and Nexus ZIPs no longer carry `HeadTracking.ini`. The mod creates `CameraUnlock.ini` when it first starts.

### Removed

- The sensitivity, scale, deadzone and axis inversion settings: `[Sensitivity] Yaw`, `Pitch`, `Roll`, `InvertYaw`, `InvertPitch` and `InvertRoll`, `[Deadzone] Yaw`, `Pitch` and `Roll`, and `[Position] WorldScale`, `SensX`, `SensY`, `SensZ`, `InvertX`, `InvertY` and `InvertZ`. Set these in your tracker app instead. Every one shipped at its identity value, and the 40 world units per metre the mod shipped as `WorldScale` is now part of its own axis conversion, so with these settings at their shipped values the camera moves as it did before.
- `[Hotkeys] DebounceMs`, which no version of the mod applied.
- `[Position] ZoomReference` and `ZoomScaleMax`, which set how leaning scaled with the camera's zoom. Leaning now always takes the first zoom the game shows as its reference and scales by at most 2.5 times either way, which is what both shipped at, so with them at their shipped values the camera moves as it did before.

## [0.2.1] - 2026-09-16

### Added

- rename the debug log and make it readable for bug reports

### Fixed

- record the cameraunlock-core commit that is actually built
- mirror the vertical limit and restore the MIT grant
- re-sync THIRD-PARTY-NOTICES.md before cutting the tag
- rewrite install.cmd MOD_VERSION from the canonical version
- resolve the adversarial review findings
- name itsloopyo in the reproduced cameraunlock-core licence
- preserve originals across shim upgrades and uninstall failures

## [0.2.0] - 2026-08-20

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

### Changed

- Updated the bundled `cameraunlock-core` submodule.

## [0.1.1] - 2026-06-07

### Changed

- Updated the bundled `cameraunlock-core` submodule.

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
