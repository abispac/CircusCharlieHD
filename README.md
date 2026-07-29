# Big Top Run Native

Clean-room native arcade prototype inspired by the timing and challenge style
of early circus obstacle games. It does not emulate hardware, load ROMs, or
contain extracted graphics, audio, program code, names, or logos.

The current milestone is a playable Stage 1 physics prototype with original
vector placeholder art. Final production art will be authored as HD source
assets and rendered directly at the active display resolution.

## Display design

- Portrait gameplay world: `480 × 640` logical units.
- Exact fixed simulation cadence: approximately `60.606060 Hz`, matching the
  documented original board timing.
- Rendering resolution is dynamic. The logical units do not limit art detail.
- Modern landscape displays pillarbox the portrait playfield.
- A vertically mounted CRT can use a landscape signal and rotate the final
  frame in software.
- Low-detail rendering automatically activates at 320-line output.

## Build on macOS

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

Portrait development window:

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
./build/big_top_run
```

Vertical arcade CRT using a `640 × 480` landscape signal:

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
./build/big_top_run --mode 640x480 --rotate 90 --fullscreen
```

Progressive 240p cabinet mode:

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
./build/big_top_run --mode 320x240 --rotate 90 --fullscreen
```

Modern 1080p display:

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
./build/big_top_run --mode 1920x1080 --fullscreen
```

## Controls

- `Enter` or `1`: start
- `Left` / `A`: slow down
- `Right` / `D`: speed up
- `Space`, `Z`, or arcade button 1: jump
- `R`: restart event
- `F1`: physics/debug overlay
- `F11`: toggle fullscreen
- `Escape`: return to title, then quit

SDL game controllers are detected automatically. D-pad/left stick controls
speed and the south face button jumps.

## CRT note

A 15-kHz arcade monitor normally displays `640 × 480` as interlaced video.
For progressive output, use a compatible 240p mode such as `320 × 240` and
CRT Emudriver/SwitchRes-compatible hardware. Never send an unsupported scan
rate to an arcade monitor.
