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
2. Copy `HeadTracking.dll`, `bw-headtracking-launcher.exe`, and `HeadTracking.ini` next to `runblack.exe`.
3. Launch the game via `bw-headtracking-launcher.exe` instead of `runblack.exe`.

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

It starts on 6DOF unless `Position.Enabled=false`, in which case it starts on rotation only.

The nav-cluster keys are configurable in `HeadTracking.ini`; the chord bindings are fixed.

## Configuration

Settings live in `HeadTracking.ini`, placed next to `runblack.exe`. Edit with any text editor; changes take effect on next game launch.

A comment has to sit on its own line, above the key. The parser hands the whole
text after `=` to the value reader. For a `true`/`false` or text setting that
text is compared as a whole, so a trailing `; note` makes the comparison fail
and the setting silently keeps its default. Numeric settings survive a trailing
comment because the number is read off the front of the text, which is why some
lines below still carry one. Putting every comment on its own line always works.

```ini
[Network]
Port=4242
EnableOnStartup=true

[Sensitivity]
Yaw=1.0
Pitch=1.0
Roll=1.0
InvertYaw=false
InvertPitch=false
InvertRoll=false

[Smoothing]
; Covers rotation and position alike. Which value is used is picked per
; connection from where the tracker sends from. 0.0 instant, up to 0.99 max,
; and nothing floors either.
LocalSmoothing=0.0    ; tracker running on this PC
RemoteSmoothing=0.15  ; tracker on the network, e.g. a phone over WiFi

[Deadzone]
Yaw=0.0
Pitch=0.0
Roll=0.0

[Position]
Enabled=true
WorldScale=40.0    ; engine units per metre of head movement - main tuning knob
ZoomReference=0.0  ; focal distance WorldScale is tuned at; 0 = auto-lock first zoom
ZoomScaleMax=2.5   ; clamp on zoom scaling [1/max, max]; lower if zoom-out too strong
SensX=1.0
SensY=1.0
SensZ=1.0
InvertX=false
InvertY=false
; InvertZ is for a tracker that sends depth backwards, not for a lean that
; feels reversed. It is applied before the LimitZ / LimitZBack clamp, so
; turning it on also swaps the travel budgets to 0.10m forward and 0.40m back.
InvertZ=false
LimitX=0.30        ; movement envelope in metres, before world scaling
LimitY=0.20
LimitZ=0.40        ; forward lean (generous)
LimitZBack=0.10    ; backward lean (restricted)

[Hotkeys]
Toggle=0x23      ; VK_END
YawMode=0x22     ; VK_NEXT (Page Down) - toggle world vs camera-local yaw
ModeCycle=0x21   ; VK_PRIOR (Page Up) - cycle 6DOF -> rotation-only -> position-only
DebounceMs=200

[View]
; true = horizon-locked yaw (default), false = camera-local
WorldSpaceYaw=true
```

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

- Raise the smoothing value your tracker actually uses in `HeadTracking.ini`: `[Smoothing] LocalSmoothing` if it runs on this PC, `[Smoothing] RemoteSmoothing` if it is a phone or other device on the network (try `0.3` to start). The log records which of the two is in effect.
- Increase per-axis `Deadzone` values to suppress micro-movements near center.
- If using a phone app, prefer sending directly to port 4242 rather than relaying via OpenTrack.

**Positional tracking feels too strong / too weak / wrong direction**

- `Position.WorldScale` is the master knob: lower it if leaning lurches the camera, raise it until the shift is noticeable. It converts metres of head movement into engine units.
- Positional tracking automatically scales with zoom so it feels the same zoomed in or out. By default (`ZoomReference=0`) it locks to the zoom level you're at when tracking first applies, and scales relative to that. If you want a fixed reference, set `[Logging] PositionTrace=true` in `HeadTracking.ini`, play for a moment at your preferred zoom, read the `focal=` value from `HeadTracking.log`, and set `ZoomReference` to it.
- `ZoomScaleMax` caps how far the zoom scaling can push (range `[1/max, max]`). B&W's focal distance spans roughly 500x across the zoom range, so without a cap the camera lunges at full zoom-out. If zoom-out still feels too strong, lower `ZoomScaleMax` (e.g. `1.8`); if zoom-out feels too weak, raise it.
- Flip `InvertX` or `InvertY` if an axis pushes the view the wrong way. `InvertZ` is for a tracker that sends depth backwards, not for a lean that feels reversed: it is applied before the `LimitZ` / `LimitZBack` clamp, so switching it on also swaps the travel budgets to 0.10m forward and 0.40m back.
- Set `Position.Enabled=false` to disable 6DOF and keep rotation only.
- Position is applied last, as a camera-local shift (relative to where you're looking).

**Yaw feels wrong at extreme pitch**

- Toggle between world-locked and camera-local yaw with `Page Down` (or `Ctrl+Shift+H`). World-locked keeps "up" constant; camera-local follows the camera's up-axis and produces a lean feel when looking up or down.

**Objects pop in at the screen edges when looking around**

- This is a limitation of the 2001 engine, not a bug in the mod. Black & White decides which objects to draw against the camera the game logic uses, which head tracking deliberately leaves unrotated so that aim, cursor picking, and AI stay correct. Turning your head to look past the normal screen edge can reveal objects the engine had culled. Fixing it would require rotating the camera the game itself reads, which would break aim and interaction, so it is left as-is.
- Lowering yaw/pitch sensitivity reduces how far you can look past the edge, which lessens the effect.

## Updating

Download the new release and run `install.cmd` again. Your `HeadTracking.ini` is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod DLL, launcher, and INI from the game directory.

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
| `pixi run package`       | Create release ZIP                     |
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
