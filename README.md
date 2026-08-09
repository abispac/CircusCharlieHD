# Big Top Run Native

Clean-room native arcade prototype inspired by the timing and challenge style
of early circus obstacle games. It does not emulate hardware, load ROMs, or
contain extracted graphics, program code, names, or logos. User-supplied
soundtrack and effects are loaded only from the ignored local `assets/audio`
folder and are not stored in this repository.

The current milestone includes playable Event 1, Event 2, and an Event 3
tambourine first pass, plus a faithful six-event HD selection screen.
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
- `1` or `Enter`: open the event-selection screen when a credit is available
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
and the Start/Menu button begins or confirms event selection.

## Event selection

The six-event screen follows the recorded arcade layout and starts with
Charlie on Event 1. Only `Left`/`Right`, `A`/`D`, or the controller D-pad's
left/right directions move Charlie. The selection cycles through Events 1–6
in order and wraps at both ends; up/down input does nothing. `Space`, `Z`,
`1`, `Enter`, controller A, or controller Start confirms the current cell.
Track 02 plays once and automatically confirms the cell where Charlie is
standing when it ends. Confirmation consumes the credit. Events 1–3 launch
their own courses; later cells temporarily route to Event 1 until implemented.

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

## Local audio

The local build uses these standardized 48 kHz mono files:

- `event-select.wav`: track 02, played once during the six-event screen; its
  exact WAV duration controls automatic confirmation
- `event-select-move.wav`: isolated command `0x52` (catalog RMS 927),
  restarted on every valid move between event cells
- `event-select-confirm.wav`: isolated command `0x4d` (catalog RMS 495),
  played when the current event is confirmed
- `event1-stage.wav`: track 03, looped during Event 1, stopped immediately on
  a miss or goal arrival, and restarted from the beginning when Charlie
  respawns. Its playback switches to the arcade-style double-speed warning
  as soon as the Zeppelin bonus reaches `0999`.
- `jump.wav`: track 08, restarted on every valid jump
- `event3-stage.wav`: full track 06 (`Trombonanza`), looped during Event 3
- `stage3-bounce.wav`: isolated command `0x46` (catalog RMS 1279), played on
  ordinary tambourine rebounds; the fourth over-jump uses RMS 1239
- `miss.wav` (isolated command `0x4f`, RMS 1239) and `miss-2.wav`: started together on every
  fire collision
- `crowd-cheer.wav`: played once when Charlie and the lion reach the goal
- `bird-coin-drop.wav`: played with the perfect-clear coin shower
- `bonus-count.wav`: played while the remaining time is counted on the bonus
  screen
- `extra-charlie.wav`: isolated command `0x42` (catalog RMS 1007), played only
  when the extra Charlie is actually collected
- `prize-bag.wav`: isolated command `0x49` (catalog RMS 1567)
- `hidden-coin.wav`: optional, separate reward effect.
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

Pass `--capture-scene start`, `--capture-scene select`, `--capture-scene stage3`, `--capture-scene stage3-approach`, `--capture-scene stage3-roof`,
`--capture-scene ring`, `--capture-scene extra`, or `--capture-scene crash`
to inspect those animation states; `goal` and `tally` are also available.

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
