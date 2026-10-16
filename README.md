# Circus Charlie HD

A native remake of Konami's 1984 *Circus Charlie* with hand-painted HD art
and the original board's behaviour: every event is transcribed from the
`circusc4` program and verified frame by frame against MAME captures. C++20
and SDL2, one 480 × 640 portrait playfield, runs on macOS, Windows and
Linux and on a vertical cabinet monitor.

All six events are playable: the lion and the rings of fire, the tightrope
and the monkeys, the trampolines (with the water round on the second visit),
the rolling balls, the pony and the springboards, and the trapeze.

## Play

Ready-to-play builds are on the
[Releases](https://github.com/abispac/CircusCharlieHD/releases) page:

- `CircusCharlieHD-windows-x64.zip` — unzip, double-click
  `CircusCharlieHD.exe`.
- `CircusCharlieHD-macOS-arm64.zip` — unzip, open
  **Circus Charlie HD.app** (signed and notarized; Apple Silicon).

### Arcade flow

Power on shows the boot screen, then the attract loop: a recorded stage,
the "insert coin" reel, another stage, and so on. Insert a coin and press
START to play Event 1 straight away. Clear the six events in a row, trapeze
included, and the event-selection screen unlocks: Events 1–5 can each be
picked five times before they lock, and the trapeze never ends — every
round gets harder, like the board. On the second and fourth visit the
trampolines chain straight into the water round. Play counts and the
unlock are saved between runs; credits last only for the current session.

The high score and the play counts live in SDL's per-user preferences
folder (`high-score.txt`, `progress.txt`). The first extra life comes at
20,000 points and another every 70,000 after that.

### Controls

| Key | Action |
| --- | --- |
| `5` or `C` | insert a coin |
| `1` or `Enter` | START |
| `Left` / `Right` (`A` / `D`) | move; on the pony, change pace; on the trapeze, pump the swing |
| `Space` or `Z` | jump / release the bar |
| `R` | restart the current event |
| `F10` | rotate the picture 90° (0 → 90 → 180 → 270) |
| `F11` | fullscreen |
| `Esc` | during play: back to the coin screen; attract mode / coin screen: quit |

`5` and `1` are the MAME mappings, so a cabinet's USB encoder works as is.
SDL game controllers are detected automatically: D-pad or left stick moves,
the south button jumps, Back/View inserts a coin, Start/Menu starts.

### Cabinets and vertical monitors

The game always draws at 480 × 640 and scales that picture to the display,
so a monitor mounted in portrait fills the screen with no options at all.
A landscape panel turned on its side wants the picture rotated:

```sh
circus_charlie_hd --fullscreen
circus_charlie_hd --rotate 90 --fullscreen       # or 270, depending on which way it was turned
circus_charlie_hd --render 240x320 --fullscreen  # low-resolution cabinet: low-detail art, integer scaling
circus_charlie_hd --render native --fullscreen   # draw at the display's full resolution
circus_charlie_hd --mode 320x240 --rotate 90 --fullscreen   # progressive 240p on a 15 kHz CRT
circus_charlie_hd --event 4                       # jump straight to an event
```

A 15 kHz arcade monitor shows 640 × 480 interlaced; never send it a scan
rate it does not support. `--help` lists every option.

## Build from source

```sh
git clone https://github.com/abispac/CircusCharlieHD.git
cd CircusCharlieHD
```

macOS (Homebrew):

```sh
brew install cmake ninja sdl2 sdl2_image
cmake -S . -B build -G Ninja
cmake --build build
./build/circus_charlie_hd
```

Linux (Debian/Ubuntu):

```sh
sudo apt install cmake ninja-build g++ libsdl2-dev libsdl2-image-dev
cmake -S . -B build -G Ninja
cmake --build build
./build/circus_charlie_hd
```

Windows: the same CMake project builds with Visual Studio or MSYS2 once SDL2
and SDL2_image are installed (`vcpkg install sdl2 sdl2-image` or
`pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image`). The
release .exe is cross-compiled from Linux or macOS with
`tools/build_windows.sh` (see below).

The build copies `assets/` next to the binary; the game looks for the
folder in the working directory first and next to the executable second.

## Release packages

- `tools/package_macos.sh` (run on a Mac, needs `brew install dylibbundler`)
  builds in Release, copies the SDL libraries into the app bundle, signs it
  and writes `dist/CircusCharlieHD-macOS-<arch>.zip`. With
  `SIGN_IDENTITY="Developer ID Application: …"` and `NOTARY_PROFILE=…` set
  it signs with the Developer ID certificate (hardened runtime) and
  notarizes and staples the app; without them it ad-hoc signs, which runs
  but needs a right-click > Open on other Macs.
- `tools/build_windows.sh` (Linux or macOS with `mingw-w64`) downloads the
  SDL2/SDL2_image MinGW packages, cross-compiles `CircusCharlieHD.exe` and a
  console variant for diagnostics, and writes
  `dist/CircusCharlieHD-windows-x64.zip` with the assets and the launcher
  `.bat` files from `dist-extras/windows/`.

## Repository layout

| Path | What it is |
| --- | --- |
| `src/main.cpp` | engine, Events 1–4, the attract/arcade flow, rendering, audio |
| `src/level5_*.inc`, `src/level6_*.inc` | Events 5 and 6 (types, logic, drawing) |
| `assets/*.png` + `.json` | runtime atlases (the JSON next to each atlas records its cells) |
| `assets/audio/` | the sound set (48 kHz mono WAV) |
| `assets/boot/`, `assets/between/`, `assets/attract/` | boot reel, "insert coin" reel and the six recorded stage clips of the attract loop |
| `art/source/` | the painted sheets the atlases were cut from (`incoming/` holds the latest generations) |
| `concepts/` | concept art |
| `docs/` | the per-event board models, calibration notes and the replay captures used to verify them |
| `tools/` | sprite importer, scenery renderers, MAME capture scripts and the replay comparers |
| `dist-extras/` | files bundled into the release zips |

## Contributing

The game is feature-complete; what it needs is polish — cleaner sprites,
better animation, scenery — and testing on real cabinets. Open an issue or
a pull request. Two rules keep the project honest:

1. **The board is the reference.** Timing, positions, scores and sounds
   come from the `circusc4` program, not from feel. Every event has a
   replay test (below); a change to `src/` must still replay with no
   mismatches.
2. **Art goes through the importer.** New sheets are cut into atlases with
   `tools/import_sprites.py` so cells stay aligned and the game keeps
   measuring them the same way.

### Sprite pipeline

Generated sheets (Gemini, GPT Image, any green-screen or transparent sheet
of any layout) go to `art/source/incoming/` and become uniform atlases
with

```sh
python3 tools/import_sprites.py SHEET.jpeg assets/NAME-N-vK.png [--select ...] [--columns N] \
    [--anchor feet|top|center] [--scale F] [--chroma] [--cut-above-body] [--preview P.png]
```

The background is keyed, every frame is found, cropped by its own pixels
and placed in a cell with its feet on the baseline (or hung from the hands,
or centred), with a JSON file of the frame boxes next to the atlas.
`--chroma` keys a pure green screen by its green excess and despills only
the fringe, so green and olive clothes survive. In the game,
`measureSheetCells` reads the opaque bounds of each cell and
`drawStage2Pose`, `drawPoseFromTop` and friends place the frame by its feet
or hands. Atlases are versioned (`-v2`, `-v3`): add a new version, point
the source at it, keep the old one out of the repository.

`tools/render_stage2_backdrop.py`, `tools/render_stage3_water_art.py` and
`tools/render_stage6_scenery.py` draw the vector scenery (`pip install
pillow cairosvg`).

### Attract clips

The attract loop plays the six JPEG clips in `assets/attract/stage1..6/`.
To re-record one, play the event and let Charlie fall; the recorder stops
by itself:

```sh
./build/circus_charlie_hd --event 3 --record assets/attract/stage3
```

`--record-fps` (default 20) and `--record-seconds` (cap, default 60) are
optional. The boot and "insert coin" reels in `assets/boot/` and
`assets/between/` are frame sequences cut with ffmpeg from cabinet
recordings; the game plays them from frames so it ships no video decoder.

### Audio

`assets/audio/` holds the standardized 48 kHz mono set: the stage tracks
(`event1-stage.wav` … `event6-stage.wav`, looped), `jump.wav`, `miss.wav`,
`fail.wav`, `crowd-cheer.wav`, `coin.wav`, the selection-screen cues and the
per-event effects. The game stays playable, silent, when a file is missing.
`tools/capture_sound_ids.lua` asks MAME's emulated sound board to render
every command in isolation, `tools/split_sound_sweep.py` writes one WAV per
command and `tools/build_sound_catalog.py` builds a browser page to audition
them; `tools/trace_sound_commands.lua` maps an in-game action to its exact
command number.

### Verifying against the board

Each event has a board model in `docs/LEVEL<n>_ROM_MODEL.md` and a set of
MAME captures in `docs/diagnostics/level<n>-replay/`. A capture is a CSV of
the joystick column and the board's state for every frame;
`tools/autoplay_level<n>_headless.lua` produces new ones in headless MAME:

```sh
mame circusc4 -video none -sound none -nothrottle -seconds_to_run 0 -skip_gameinfo \
     -autoboot_script tools/autoplay_level3_headless.lua
```

The native game replays the joystick column and writes its own state, and
the comparer checks every column of every playing frame:

```sh
./build/circus_charlie_hd --replay docs/diagnostics/level3-replay/auto-goal.csv \
     --replay-event 3 --replay-output /tmp/native.csv
python3 tools/compare_level3_replay.py docs/diagnostics/level3-replay/auto-goal.csv /tmp/native.csv
```

Captures made with a special mode need the matching flag:
`--replay-perfect` (Event 2 perfect clears), `--replay-clear-projectiles`
and `--replay-invulnerable` (Event 3), `--replay-water` plus
`--replay-invulnerable` (the water round), `--replay-visits N` (a later
visit). All captures in the repository replay with no mismatches; keep it
that way.

#### Event 1

Event 1 runs the `circusc4` board model documented in
`docs/LEVEL1_ROM_MODEL.md`: course progress as page/offset, the full course
stream, the three fire-pot records, the hidden coin, the extra Charlie, the
goal, the coin shower and the failure restart all follow the disassembled
routines. `--replay capture-state.csv --replay-output native.csv
--replay-frame-byte N` feeds the joystick column of a MAME capture into the
native game; `tools/compare_level1_replay.py` then compares every board frame
with the emulated state (`tools/compact_level1_objects.py` reduces the
object capture for it). The captures in `docs/diagnostics/level1-replay/`
replay with no mismatches.

#### Event 2

Event 2 runs the `circusc4` tightrope handler documented in
`docs/LEVEL2_ROM_MODEL.md`: the half-column scroll, the `$F2C0` monkey
schedule, the brown and purple walks, the purple leap over a brown, the
`$80C2` collision, the 57-frame jump arc, the `$8636` scoring with the turned
monkey and its number, the goal stand with its 5000 and the perfect-clear bird
that brings the extra Charlie. Charlie is never mirrored on the rope (the
sprite attribute stays `$20` walking either way), the renderer draws every
record where the board puts it, and the sounds follow the MAME latch trace
(`$44` at takeoff, `$49` on a combo landing, `$4F` on a hit). The seven
captures in `docs/diagnostics/level2-replay/` replay with no mismatches
(`CIRCUS_L2_PERFECT=1` captures need `--replay-perfect`).

#### Event 3

Event 3 runs the `circusc4` trampoline handler documented in
`docs/LEVEL3_ROM_MODEL.md`: the two-column scroll with the `$F8` walk-in
page, the 8.8 rebound velocities (`$0420`/`$03C0` moving, `$FA65`
stationary) with the roof on the fourth stationary apex, the `$F517`
performer schedule, fire-breather flames and juggled knives with their
collision windows, the seven `$FA43` bags worth 300-900, the 20-point
landings, the 160-frame celebration with the bird and forty 100-point
coins for a perfect clear, the seven-frame hit, the fall, the 40-frame
fallen pose, the page step-back and 96-frame restart with the 4000/3500/
3000 bonus. The water round of the second visit (`$2207`: the drums stand
in a pool, dolphins take the performers' records and only their bodies are
dangerous) follows the same model; `--event 3 --water` starts it,
`CIRCUS_L3_WATER=1` captures it in MAME and `--replay-water` replays those
captures. Its scenery is drawn by `tools/render_stage3_water_art.py`.

#### Event 4

Event 4 runs the `circusc4` rolling-ball handler documented in
`docs/LEVEL4_ROM_MODEL.md`: the one-column scroll with the `$F8` walk-in page,
the 57-frame jump arc, the `$9A42` landing window and the record swap that
makes the ball landed on the ridden one, the `$F1B5` spawn schedule that
tightens with every landing, the close-ball bonus worth 400 to 2000 points,
the 192-frame idle slip, the ball-versus-ball collision and the goal stand.

#### Event 5

Event 5 runs the `circusc4` pony-and-springboards handler documented in
`docs/LEVEL5_ROM_MODEL.md`: the three paces of the pony, the `$F67C`
springboard schedule that the difficulty dips and the visit count start
into, the doubles every fourth spawn, the landing window and the diagonal
pole test, the BCD landing points that double with every pace step, the
tumble after a double bounce, the goal stand and the extra-life bird.
`tools/autoplay_level5_headless.lua`'s `search` mode finds a clear with
save states.

#### Event 6

Event 6 runs the `circusc4` trapeze handler documented in
`docs/LEVEL6_ROM_MODEL.md`. The handler works on RAM — five trapeze
records, the sprite table and the direct page — so the port keeps a byte
mirror of the board's work RAM and transcribes each routine against it: the
swing phase and amplitude tables, the pump on the stick, the release with
the bar's averaged velocity, the catch window and its points, the
trampolines and their rebounds, the sixteen-swing drop, the goal platform on
page 6 and the three-act celebration. `tools/autoplay_level6_headless.lua`'s
`search` mode finds a clear.

### Display model

- Portrait world of `480 × 640` logical units; the board's `224 × 256`
  upright image maps to it with `kSourceToWorldX = 480/224` and
  `level3RowToY(r) = r * 2.5 − 8`.
- Fixed simulation cadence of `6,144,000 / (384 × 264) ≈ 60.606 Hz`, the
  board's own.
- The marquee and the score panel stay fixed; the arena, crowd, floor and
  goal scroll with Charlie; landscape displays pillarbox the playfield.
- Low-detail drawing and nearest-neighbour scaling switch on at 320-line
  output.

`docs/ARCADE_CALIBRATION_WORKFLOW.md` describes how reference frames and
the remake are compared on the same grid before anything is tuned;
`docs/REFERENCE_MEASUREMENTS.md`, `docs/PHYSICS.md` and
`docs/ROM_ANALYSIS.md` hold the board measurements and the disassembly
notes the models are built on.

## About the original

*Circus Charlie* is © Konami. This project is a fan remake made for the
love of the game; it does not include or load the arcade program, and the
board's behaviour was reproduced by measurement. The boot and "insert coin"
reels and the sound set were recorded from the arcade game.
