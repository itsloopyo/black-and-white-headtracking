# Black & White Head Tracking

An unofficial head tracking mod for Black & White (Lionhead Studios, 2001) that adds free look on top of the game's mouse-driven gameplay using any OpenTrack-compatible tracker.

![Mod GIF](https://media.githubusercontent.com/media/itsloopyo/black-and-white-headtracking/main/assets/readme-clip.gif)

## Features

- **Decoupled look and aim** - head tracking adds free look while your mouse cursor stays in place in the world
- **6DOF positional tracking** - lean and peek with head position

## Requirements

- A legally owned working install of [Black & White](https://en.wikipedia.org/wiki/Black_%26_White_(video_game)) (Lionhead Studios, 2001) with [Unofficial Fan Patch 1.42](https://www.bwgame.net/downloads/black-white-unofficial-patch-v1-42.1418/)
- [OpenTrack](https://github.com/opentrack/opentrack) or any OpenTrack-protocol compatible tracker (webcam, phone app, hardware tracker).
- Windows 10 or 11.

## Installation

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

In OpenTrack's main window, set **Output** to `UDP over network` and configure:

- IP: `127.0.0.1`
- Port: `4242`

Start OpenTrack before launching the game (or any time during play - the mod reconnects automatically).

### VR Headset Setup

A VR headset makes a precise, low-latency tracker even though the game itself is not VR:

1. Connect the headset to your PC over Air Link (Quest) or [Virtual Desktop](https://www.vrdesktop.net/).
2. Start SteamVR so the headset's pose is available to other apps.
3. In OpenTrack, set **Tracker** to `SteamVR` and **Output** to `UDP over network` at `127.0.0.1:4242`.
4. Put the headset on, face forward, click **Start**, and press `Home` in-game to recenter.

### Webcam Setup

OpenTrack ships with a neuralnet tracker that works with any webcam:

1. Set **Tracker** to `Neuralnet Tracker`.
2. Click the gear icon next to the tracker and select your webcam.
3. Center yourself in front of the camera, click **Start**, and press `Home` in-game to recenter.

### Phone App Setup

If your phone tracking app (e.g. SmoothTrack, Head Tracker) can already send the OpenTrack UDP protocol, point it directly at your PC's IP on port `4242` and skip OpenTrack entirely. Apps that smooth internally generally produce a cleaner result this way.

If you want OpenTrack's curve mapping and filters, configure the phone app to send to OpenTrack instead and let OpenTrack relay to `127.0.0.1:4242`.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord          |
|---------------------|-------------|----------------|
| Recenter            | `Home`      | `Ctrl+Shift+T` |
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
Amount=0.0       ; 0.0 instant, up to 0.99 max smoothing

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
InvertZ=false
LimitX=0.30        ; movement envelope in metres, before world scaling
LimitY=0.20
LimitZ=0.40        ; forward lean (generous)
LimitZBack=0.10    ; backward lean (restricted)
Smoothing=0.15

[Hotkeys]
Recenter=0x24    ; VK_HOME
Toggle=0x23      ; VK_END
YawMode=0x22     ; VK_NEXT (Page Down) - toggle world vs camera-local yaw
DebounceMs=200

[View]
WorldSpaceYaw=true   ; true = horizon-locked yaw (default), false = camera-local

[Debug]
LogToFile=false
```

## Troubleshooting

**Mod not loading**

- Make sure you launched the game via `bw-headtracking-launcher.exe`, not `runblack.exe` directly.
- Confirm `HeadTracking.dll` sits next to `runblack.exe`.
- Set `LogToFile=true` in the `[Debug]` section and check the log next to the game executable.

**No tracking response**

- Verify OpenTrack is running and its output is set to UDP `127.0.0.1:4242`.
- Press `End` to ensure tracking is enabled, then `Home` to recenter.
- Check that no firewall is blocking local UDP traffic on port 4242.

**Jittery or unstable tracking**

- Raise `Smoothing.Amount` in `HeadTracking.ini` (try `0.3` to start).
- Increase per-axis `Deadzone` values to suppress micro-movements near center.
- If using a phone app, prefer sending directly to port 4242 rather than relaying via OpenTrack.

**Positional tracking feels too strong / too weak / wrong direction**

- `Position.WorldScale` is the master knob: lower it if leaning lurches the camera, raise it until the shift is noticeable. It converts metres of head movement into engine units.
- Positional tracking automatically scales with zoom so it feels the same zoomed in or out. By default (`ZoomReference=0`) it locks to the zoom level you're at when tracking first applies, and scales relative to that. If you want a fixed reference, enable `LogToFile`, read the `focal=` value in the log at your preferred zoom, and set `ZoomReference` to it.
- `ZoomScaleMax` caps how far the zoom scaling can push (range `[1/max, max]`). B&W's focal distance spans roughly 500x across the zoom range, so without a cap the camera lunges at full zoom-out. If zoom-out still feels too strong, lower `ZoomScaleMax` (e.g. `1.8`); if zoom-out feels too weak, raise it.
- Flip `InvertX/Y/Z` if an axis pushes the view the wrong way.
- Set `Position.Enabled=false` to disable 6DOF and keep rotation only.
- Position is applied last, as a camera-local shift (relative to where you're looking), and recenters with `Home` alongside rotation.

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

## License

MIT License - see [LICENSE](LICENSE) for details.

## Credits

- [Lionhead Studios](https://en.wikipedia.org/wiki/Lionhead_Studios) - the original Black & White.
- [OpenTrack](https://github.com/opentrack/opentrack) - head tracking software.
- [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core) - shared head tracking library.
- [MinHook](https://github.com/TsudaKageyu/minhook) - function hooking.

## Disclaimer

Not affiliated with Lionhead Studios, Microsoft, or any current rights holder of Black & White. No game files are included. Use at your own risk.
