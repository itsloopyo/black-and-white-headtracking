# Black & White Head Tracking

![Black & White running with this mod](https://media.githubusercontent.com/media/itsloopyo/black-and-white-headtracking/refs/heads/main/assets/readme-clip.gif)

*Footage captured in Black & White (Lionhead Studios, 2001), shown to demonstrate this mod. Black & White and all in-game imagery are the property of their respective rights holders. This project is not affiliated with or endorsed by them.*

An unofficial head tracking mod for Black & White that moves the camera with your head while your mouse keeps control of the cursor, driven by OpenTrack over UDP, with no VR headset required.

## Features

- **Decoupled look and aim** - head tracking adds free look while your mouse cursor stays in place in the world
- **6DOF positional tracking** - lean and peek with head position
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- A legally owned install of [Black & White](https://en.wikipedia.org/wiki/Black_%26_White_(video_game)) (Lionhead Studios, 2001) that launches and runs on your version of Windows. The 2001 release does not run as shipped on modern Windows; the fan community maintains compatibility patches for it, and the [Black & White community site](https://www.bwgame.net/) is where owners of the game go for them. Sourcing and applying those is between you and that community - this mod does not bundle, require, or endorse any particular one, and it has been developed against Unofficial Fan Patch 1.42.
- [OpenTrack](https://github.com/opentrack/opentrack) or any OpenTrack-protocol compatible tracker (webcam, phone app, hardware tracker).
- Windows 10 or 11.

## Installation

### Lopari

Download [Lopari](https://lopari.app), choose **Black & White**, and click
**Play with head tracking**.

### Standalone Installer

1. Download the latest installer ZIP from the [Releases](https://github.com/itsloopyo/black-and-white-headtracking/releases) page.
2. Extract anywhere.
3. Double-click `install.cmd`.
4. Configure OpenTrack to output **UDP over network** to `127.0.0.1:4242`.
5. Launch the game via `bw-headtracking-launcher.exe` (created next to `runblack.exe`).

The launcher starts `runblack.exe` and injects `HeadTracking.dll` into it. Any command-line arguments you pass to the launcher are forwarded to the game.

If the installer can't find your game, point it at the install root either way:

```powershell
install.cmd "D:\Games\Lionhead Studios Ltd\Black & White"
```

or set the `BLACK_AND_WHITE_PATH` environment variable before running it:

```powershell
$env:BLACK_AND_WHITE_PATH = "D:\Games\Lionhead Studios Ltd\Black & White"
install.cmd
```

The default search path is `C:\Program Files (x86)\Lionhead Studios Ltd\Black & White`.

### Manual Installation

If you'd rather place files by hand (or you're using the Nexus extract-to-folder ZIP):

1. Locate your Black & White install directory (the folder containing `runblack.exe`).
2. Copy `HeadTracking.dll` and `bw-headtracking-launcher.exe` next to `runblack.exe`.
3. Launch the game via `bw-headtracking-launcher.exe` instead of `runblack.exe`. The mod creates `CameraUnlock.ini` beside `runblack.exe` the first time it starts.

## Setting Up OpenTrack

The mod listens for OpenTrack pose data on UDP port `4242`, on every network
interface. One datagram is six little-endian 64-bit floats in the order
`x, y, z, yaw, pitch, roll`: position in centimetres, rotation in degrees, 48
bytes in total. Anything that sends that to that port drives the view.
OpenTrack's **UDP over network** output sends exactly this, and the steps below
set it up.

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Pick a tracker under **Input**, using the notes below.
3. Set **Output** to **UDP over network**, host `127.0.0.1`, port `4242`.
4. Press **Start**. Tracking and the game can start in either order.

### Webcam

OpenTrack ships a `neuralnet tracker` input that reads a plain webcam. Select it
under **Input**, pick your camera in its settings, and use the output settings
above. How well it tracks depends on your camera and your lighting, so try it
before buying anything.

### Phone

A phone app can reach the mod directly, with no OpenTrack on the PC, if it sends
the datagram described above. Point it at this PC's IP address (run `ipconfig`
to find it) on port `4242`. Not every phone tracker speaks this protocol, so
check yours for an OpenTrack or UDP output option first. [Headcam](https://headcam.app)
sends it, and I wrote it so decent tracking is free for anyone who already owns
a phone.

Sending direct works when the app filters its own signal on the device. The
mod's smoothing is sized to take the edge off a clean signal rather than to
rescue a noisy one, so a raw feed sent direct will jitter. If it does, point the
app at OpenTrack's **UDP over network** *input* on some other port, say 5252,
and let OpenTrack's filters and curves clean it up before its output forwards to
`127.0.0.1:4242`.

Anything arriving from outside `127.0.0.0/8` counts as a remote connection and
is smoothed with `RemoteSmoothing` rather than `LocalSmoothing`. That includes a
tracker on this very PC that sends to the machine's own LAN address, because the
mod reads the source address and not the machine.

### Headset or other hardware

If your device has an OpenTrack input driver, select it under **Input** and use
the same output settings. OpenTrack's own **Input** list is the authority on
what it can read; the mod only ever sees what OpenTrack sends.

### Centring

Centring belongs to your tracker. The mod subtracts no centre of its own: it
applies the pose it receives exactly as it arrives, so a stream of zeros holds
the view where the game itself puts it. Press the centre control in your tracker
(OpenTrack's **Center** bind, or the CENTER button in Headcam) and the tracker
zeroes its own output, which leaves the view centred with the mod doing nothing.

That is why there is no centre hotkey here and nothing to re-centre in game. Two
centres in series would drift apart, because each side re-centres at moments the
other cannot see, and you would end up pressing twice to centre once. If the
view sits off to one side, centre it in the tracker.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord          |
|---------------------|-------------|----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y` |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G` |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H` |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

The tracking mode and the yaw mode are saved to `CameraUnlock.ini` the moment you change them, so the game starts in them next time. `End` changes the current session only: the game starts with head tracking on or off as `EnableOnStartup` says.

Every key in the table is an entry in a key list in `CameraUnlock.ini` (`ToggleKey`, `CycleTrackingModeKey` and `YawModeKey`), the chords included, and each can be rebound or removed there. A key listed without modifiers does not fire while Ctrl and Shift are both held, so `Ctrl+Shift+End` does nothing unless a list names it.

## Configuration

Apart from creating `CameraUnlock.ini` at startup when there is none, the mod writes to it only when a hotkey changes the tracking mode or the yaw mode. It never writes `HeadTracking.ini`, and it creates `Defaults.ini` only when there is none and never changes it. Edit `CameraUnlock.ini` with the game closed.

<!-- cameraunlock:config -->
The mod reads its settings from `CameraUnlock.ini` in the game folder, and creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.

A setting that the defaults below set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it. `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.

Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:

- Reticle settings, and a key that toggled the reticle.
- A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.

An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.

Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults below. Every setting they set to `default` then follows `Defaults.ini`.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `WorldSpaceYaw=true`
- `RotationEnabled=true`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`
- `YawModeKey=PageDown, Ctrl+Shift+H`

With every setting at its default, the file reads:

```ini
; Black & White head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default
; The zoom at which leaning is not scaled, as the camera's distance to what it looks at
; in the game's units. At other zooms leaning is scaled by the ratio of the two distances,
; within ZoomScaleMax. 0 uses the first zoom the game shows. PositionTrace logs the distance.
ZoomReference=0.0
; The most the zoom scaling may multiply or divide leaning by. 1 turns it off.
; Lower it if leaning moves the view too far when zoomed out.
ZoomScaleMax=2.5

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=default

[Logging]
; true: write the tracked head position, the view's offset and the zoom to HeadTracking.log
; once a second, for the first minute of positional tracking.
PositionTrace=false
```
<!-- /cameraunlock:config -->

There are no sensitivity, inversion, deadzone or scale settings: the mod applies the pose your tracker sends, so set those in the tracker. Leaning converts at 40 game units per metre of head movement, the value earlier versions shipped as `WorldScale`.

`ZoomReference` and `ZoomScaleMax` set how leaning scales with the camera's zoom, and `PositionTrace` logs the numbers behind it (see Troubleshooting).

## Troubleshooting

**Mod not loading**

- Make sure you launched the game via `bw-headtracking-launcher.exe`, not `runblack.exe` directly.
- Confirm `HeadTracking.dll` sits next to `runblack.exe`.
- Read `HeadTracking.log` next to `runblack.exe`. Every launch starts a fresh file (the previous session is kept alongside as `HeadTracking.prev.log`), and it records the whole startup chain: the settings in effect, which hooks were installed, the UDP port, and whether any tracker packets arrived.

**No tracking response**

- Verify OpenTrack is running and its output is set to UDP `127.0.0.1:4242`.
- Press `End` to ensure tracking is enabled, then centre the view in your tracker app. The mod applies the pose it is sent as absolute and keeps no centre of its own.
- Check that no firewall is blocking local UDP traffic on port 4242.

**Jittery or unstable tracking**

- Raise the smoothing value your tracker actually uses in `CameraUnlock.ini`: `[Smoothing] LocalSmoothing` if it runs on this PC, `[Smoothing] RemoteSmoothing` if it is a phone or other device on the network (try `0.3` to start). The log records which of the two is in effect.
- Deadzones and response curves belong to your tracker; the mod has no setting for either.
- If using a phone app, prefer sending directly to port 4242 rather than relaying via OpenTrack.

**Positional tracking feels too strong / too weak / wrong direction**

- How far a lean moves the view is set by how much head movement your tracker sends; scale it there.
- Positional tracking automatically scales with zoom so it feels the same zoomed in or out. By default (`ZoomReference=0.0`) it locks to the zoom level you're at when tracking first applies, and scales relative to that. If you want a fixed reference, set `[Logging] PositionTrace=true` in `CameraUnlock.ini`, play for a moment at your preferred zoom, read the `focal=` value from `HeadTracking.log`, and set `ZoomReference` to it.
- `ZoomScaleMax` caps how far the zoom scaling can push (range `[1/max, max]`). B&W's focal distance spans roughly 500x across the zoom range, so without a cap the camera lunges at full zoom-out. If zoom-out still feels too strong, lower `ZoomScaleMax` (e.g. `1.8`); if zoom-out feels too weak, raise it.
- If an axis moves the view the wrong way, invert it in your tracker.
- Press `Page Up` (or `Ctrl+Shift+G`) to switch to rotation only. The mod saves the mode and starts in it next time.
- Position is applied last, as a camera-local shift (relative to where you're looking).

**Yaw feels wrong at extreme pitch**

- Toggle between world-locked and camera-local yaw with `Page Down` (or `Ctrl+Shift+H`). World-locked keeps "up" constant; camera-local follows the camera's up-axis and produces a lean feel when looking up or down.

**Objects pop in at the screen edges when looking around**

- This is a limitation of the 2001 engine, not a bug in the mod. Black & White decides which objects to draw against the camera the game logic uses, which head tracking deliberately leaves unrotated so that aim, cursor picking, and AI stay correct. Turning your head to look past the normal screen edge can reveal objects the engine had culled. Fixing it would require rotating the camera the game itself reads, which would break aim and interaction, so it is left as-is.
- Scaling down yaw and pitch in your tracker reduces how far you can look past the edge, which lessens the effect.

## Updating

Download the new release and run `install.cmd` again. The installer never writes `CameraUnlock.ini` or `HeadTracking.ini`, so your settings stay as they are.

## Uninstalling

Run `uninstall.cmd`. This removes the mod DLL, the launcher and the mod's logs from the game directory. `CameraUnlock.ini` and `HeadTracking.ini` stay, so your settings survive a reinstall.

This mod doesn't install a mod loader, so there's nothing extra to clean up. `uninstall.cmd /force` is accepted for consistency with other CameraUnlock mods but is a no-op here.

## Building from Source

```bash
git clone --recurse-submodules https://github.com/itsloopyo/black-and-white-headtracking.git
cd black-and-white-headtracking
pixi run install
```

The build is hard-wired to Win32 because the game is 32-bit.

| Task                     | Description                            |
|--------------------------|----------------------------------------|
| `pixi run build`         | Debug DLL + launcher                   |
| `pixi run build-release` | Release DLL + launcher                 |
| `pixi run install`       | Build + deploy to detected install     |
| `pixi run uninstall`     | Remove mod files                       |
| `pixi run test`          | Unit tests and the config differential test |
| `pixi run render-config` | Rewrite `config/HeadTracking.ini` from the config table |
| `pixi run package`       | Run the tests and create release ZIPs  |
| `pixi run validate-manifest` | Check the built ZIPs against core's manifest rules |
| `pixi run clean`         | Wipe `build/`                          |

Build dependencies (vendored or fetched): MinHook, glm, cameraunlock-core.

## Community & Support

- Discord: [Loop's Head Tracking Hangout](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch for the released head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your iPhone or Android phone into the head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

## Credits

- [Lionhead Studios](https://en.wikipedia.org/wiki/Lionhead_Studios) - the original Black & White.
- [OpenTrack](https://github.com/opentrack/opentrack) - head tracking software.
- [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core) - shared head tracking library.
- [MinHook](https://github.com/TsudaKageyu/minhook) - function hooking.

## Disclaimer

Not affiliated with, endorsed by, or sponsored by Lionhead Studios, Microsoft, or any
current rights holder of Black & White. "Black & White" and all related names, artwork,
and in-game content are the property of their respective owners and are referenced here
only to identify the game this mod works with.

No game code, game assets, or game files of any kind are included in this repository or
in any release ZIP. The mod ships only original code written for this project plus the
permissively licensed third-party libraries listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). It requires a copy of the game you
already own, it reads no game files at rest, and it circumvents no copy protection,
licence check, or DRM.

The engine addresses in `src/engine_addresses.h` and the analysis scripts in
`scripts/ghidra/` are the product of examining a legally owned copy of the game for the
sole purpose of making this mod interoperate with it. They record memory addresses and
observed behaviour; no decompiled or disassembled game code is reproduced or
redistributed here. See [scripts/ghidra/README.md](scripts/ghidra/README.md).

Use at your own risk.
