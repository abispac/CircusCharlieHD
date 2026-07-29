# Big Top Run Native

Clean-room native arcade prototype inspired by the timing and challenge style
of early circus obstacle games. It does not emulate hardware, load ROMs, or
contain extracted graphics, audio, program code, names, or logos.

The current milestone is a playable Stage 1 prototype with original
hand-painted HD rider, arena, and fire-hoop assets. Vector rendering remains
only as a fallback if an asset cannot be loaded.

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
brew install cmake ninja sdl2 sdl2_image
```

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
- `Left` / `A`: move or backtrack left
- `Right` / `D`: move right
- Release horizontal input: stop
- `Space`, `Z`, or arcade button 1: jump
- `R`: restart event
- `F1`: physics/debug overlay
- `F11`: toggle fullscreen
- `Escape`: return to title, then quit

SDL game controllers are detected automatically. D-pad/left stick controls
direction and the south face button jumps.

The title screen and the opening gameplay overlay show these controls. They
are not hidden in this file.

## Capture a deterministic preview

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
./build/big_top_run --mode 480x640 --capture /tmp/big-top-preview.png
```

## Reference analysis

The supplied ROM can be run locally through MAME for behavioral measurement.
The tools under `tools/` capture deterministic input traces and ask MAME to
disassemble its already-decrypted KONAMI-1 opcode view. Temporary ROM-derived
screenshots, disassembly, and memory maps are written only to `/tmp` and are
never committed or redistributed.

## CRT note

A 15-kHz arcade monitor normally displays `640 × 480` as interlaced video.
For progressive output, use a compatible 240p mode such as `320 × 240` and
CRT Emudriver/SwitchRes-compatible hardware. Never send an unsupported scan
rate to an arcade monitor.
