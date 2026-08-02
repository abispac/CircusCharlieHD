# Big Top Run Native

Clean-room native arcade prototype inspired by the timing and challenge style
of early circus obstacle games. It does not emulate hardware, load ROMs, or
contain extracted graphics, program code, names, or logos. User-supplied
soundtrack and effects are loaded only from the ignored local `assets/audio`
folder and are not stored in this repository.

The current milestone is a playable Stage 1 prototype with original
hand-painted HD rider, green Stage 1 arena, marquee, Ferris-wheel, and
fire-hoop assets.
The friendly blond Charlie rider uses a six-pose sheet while retaining the
original HD lion artwork. Vector rendering remains only as a fallback if an
asset cannot be loaded.

## Display design

- Portrait gameplay world: `480 × 640` logical units.
- Exact fixed simulation cadence: approximately `60.606060 Hz`, matching the
  documented original board timing.
- Rendering resolution is dynamic. The logical units do not limit art detail.
- The upper zeppelin/tent marquee and black score panel remain fixed.
- The Ferris wheel rotates in place while its gondolas stay upright.
- The lower arena wall, crowd, floor, distance signs, and goal scroll with
  Charlie.
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

- `5` or `C`: insert one coin / add one credit
- `1` or `Enter`: one-player start; consumes one credit
- `Left` / `A`: move or backtrack left
- `Right` / `D`: move right
- Release horizontal input: stop
- `Space`, `Z`, or arcade button 1: jump
- `R`: restart event
- `F1`: physics/debug overlay
- `F11`: toggle fullscreen
- `Escape`: return to the coin waiting screen, then quit

SDL game controllers are detected automatically. D-pad/left stick controls
direction, the south face button jumps, the Back/View button inserts a coin,
and the Start/Menu button consumes one credit to begin.

The cabinet-standard keyboard mappings are `5` for coin and `1` for start, so
a physical coin acceptor can use a normal arcade USB encoder configured like
MAME. The temporary prototype title and repeated gameplay instruction overlay
have been replaced by an arcade-style credit waiting screen.

## Score memory and credits

The high score is retained between launches in SDL's per-user application
preferences directory as `high-score.txt`. Credits intentionally last only
for the current power session, like a conventional arcade board. The first
score-based extra life is awarded at 20,000 points and another is awarded
every 70,000 points after that.

## Planned boot and attract sequence

The present coin waiting screen is the permanent end of the future attract
flow. After all gameplay stages are complete, the recorded reference will be
used to reproduce the board boot through the logo and animated stars, then a
new HD montage featuring every completed stage will lead into this credit
screen. That later montage is intentionally deferred until every stage has
final art and animation available.

## Local Event 1 audio

The local build uses these standardized 48 kHz mono files:

- `event1-stage.wav`: track 03, looped during Event 1, stopped immediately on
  a miss or goal arrival, and restarted from the beginning when Charlie
  respawns. Its playback switches to the arcade-style double-speed warning
  as soon as the Zeppelin bonus reaches `0999`.
- `jump.wav`: track 08, restarted on every valid jump
- `miss.wav` and `miss-2.wav`: tracks 07 and 09, started together on every
  fire collision
- `crowd-cheer.wav`: played once when Charlie and the lion reach the goal
- `bird-coin-drop.wav`: played with the perfect-clear coin shower
- `bonus-count.wav`: played while the remaining time is counted on the bonus
  screen
- `extra-charlie.wav`: isolated command `0x42` (catalog RMS 1007), played only
  when the extra Charlie is actually collected
- `prize-bag.wav` and `hidden-coin.wav`: optional, separate reward effects.
  They remain silent when absent so the known credit-insert sound is never
  substituted for a gameplay reward.
- `coin.wav`: reserved for the physical/software coin switch and played only
  when a credit is added

The game remains playable in silent fallback mode if a local audio asset is
missing. These user-supplied files stay ignored so the repository cannot
accidentally redistribute them.

## Private ROM sound catalog

`tools/capture_sound_ids.lua` asks MAME's original emulated sound board to
render every command in isolation. `tools/split_sound_sweep.py` creates one
labeled WAV per command, and `tools/build_sound_catalog.py` creates a local
browser page containing only the audible commands. The private recordings
remain outside the repository.

`tools/trace_sound_commands.lua` is the second method: start MAME with the
script, play the specific original-game action, then quit MAME. Its CSV maps
the action to the exact hexadecimal sound command, avoiding guesses based on
similar effects.

## Capture a deterministic preview

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
./build/big_top_run --mode 480x640 --capture /tmp/big-top-preview.png
```

Pass `--capture-scene start`, `--capture-scene ring`, or
`--capture-scene crash` to inspect those
animation states; `goal` and `tally` are also available.

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
