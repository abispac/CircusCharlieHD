# Circus Charlie HD

## Boot sequence

The native build opens with the supplied 16.1-second arcade boot recording,
including its original PCM audio, then transitions directly into the HD event
selection screen. You don't have to sit through it: insert a coin (`5`) and
press START (`1`), MAME style, and the video is cut short and the event
screen opens right away. The video is stored as 30 fps PNG frames under
`assets/boot/` so SDL can play it without shipping an external movie-decoder
runtime. Gameplay HUD bulbs use a moving five-color chase around all four
sides of the score panel.

Clean-room native arcade prototype inspired by the timing and challenge style
of early circus obstacle games. It does not emulate hardware, load ROMs, or
contain extracted graphics, program code, or logos. User-supplied
soundtrack and effects are loaded only from the ignored local `assets/audio`
folder and are not stored in this repository.

The current milestone includes playable Event 1, Event 2, Event 3 and Event 4,
plus a faithful six-event HD selection screen.  Events 1, 2, 3 and 4 are
frame-synchronized with the original board (see below). The
friendly blond Charlie rider uses the production Run A/B/C poses plus the
backward walking poses E/F while retaining the original HD lion artwork.
Vector rendering remains only as a fallback if an asset cannot be loaded.

## Display design

- Portrait gameplay world: `480 × 640` logical units.
- Exact fixed simulation cadence: approximately `60.606060 Hz`, matching the
  documented original board timing.
- The frame is rendered at a portrait resolution of at most `480 × 640` (the
  logical playfield) and then scaled to the display, so no monitor shows more
  than that; `--render 240x320` renders at a low-resolution cabinet's size
  (low-detail drawing and nearest-neighbour scaling switch on at 320 lines)
  and `--render native` uses the display's full resolution.
- The upper zeppelin/tent marquee and black score panel remain fixed.
- The Ferris wheel rotates in place while its gondolas stay upright.
- The lower arena wall, crowd, floor, distance signs, and goal scroll with
  Charlie.
- Modern landscape displays pillarbox the portrait playfield.
- A vertically mounted CRT can use a landscape signal and rotate the final
  frame in software.
- Low-detail rendering automatically activates at 320-line output.

## Build on macOS

Every command below runs from the repository root.

```sh
git clone https://github.com/abispac/CircusCharlieHD.git
cd CircusCharlieHD
brew install cmake ninja sdl2 sdl2_image
```

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

Portrait development window:

```sh
./build/circus_charlie_hd
```

Vertical arcade CRT using a `640 × 480` landscape signal:

```sh
./build/circus_charlie_hd --mode 640x480 --rotate 90 --fullscreen
```

Progressive 240p cabinet mode:

```sh
./build/circus_charlie_hd --mode 320x240 --rotate 90 --fullscreen
```

Any portrait monitor drawn at 240x320 and scaled up:

```sh
./build/circus_charlie_hd --render 240x320 --fullscreen
```

Modern 1080p display:

```sh
./build/circus_charlie_hd --mode 1920x1080 --fullscreen
```

## Controls

- `5` or `C`: insert one coin / add one credit
- `1` or `Enter`: open the event-selection screen when a credit is available
  (during the boot video this also skips the rest of it)
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
standing when it ends. Confirmation consumes the credit. Events 1–4 launch
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
  when the Zeppelin bonus reaches `0499`, the value tested by the board at
  `$BB73`.
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
./build/circus_charlie_hd --mode 480x640 --capture /tmp/circus-charlie-preview.png
```

Pass `--capture-scene start`, `--capture-scene select`, `--capture-scene stage3`, `--capture-scene stage3-approach`, `--capture-scene stage3-roof`,
`--capture-scene ring`, `--capture-scene extra`, or `--capture-scene crash`
to inspect those animation states; `goal` and `tally` are also available.

## Level 1 board model and replay verification

Event 1 now runs the `circusc4` board model documented in
`docs/LEVEL1_ROM_MODEL.md`: course progress as page/offset, the full course
stream, the three fire-pot records, the hidden coin, the extra Charlie, the
goal, the coin shower and the failure restart all follow the disassembled
routines. `--replay capture-state.csv --replay-output native.csv
--replay-frame-byte N` feeds the joystick column of a MAME capture into the
native game; `tools/compare_level1_replay.py` then compares every board frame
with the emulated state (`tools/compact_level1_objects.py` reduces the
object capture for it). The two manual captures and the headless
three-failure run in `docs/diagnostics/level1-replay/` replay with no
mismatches. `tools/autoplay_level1_headless.lua` produces new captures in
headless MAME (`-video none -sound none -nothrottle`).

## Event 2 board model and replay verification

Event 2 runs the `circusc4` tightrope handler documented in
`docs/LEVEL2_ROM_MODEL.md`: the half-column scroll, the `$F2C0` monkey
schedule, the brown and purple walks, the purple leap over a brown, the
`$80C2` collision, the 57-frame jump arc, the `$8636` scoring with the turned
monkey and its number, the goal stand with its 5000 and the perfect-clear bird
that brings the extra Charlie.  Charlie is never mirrored on the rope (the
sprite attribute stays `$20` walking either way), the renderer draws every
record where the board puts it, and the sounds follow the MAME latch trace
(`$44` at takeoff, `$49` on a combo landing, `$4F` on a hit).  `--replay
capture.csv --replay-event 2 --replay-output native.csv` (with
`--replay-perfect` for a capture made with `CIRCUS_L2_PERFECT=1`) replays a
`tools/autoplay_level2_headless.lua` capture and
`tools/compare_level2_replay.py` compares every column of every playing frame;
the seven captures in `docs/diagnostics/level2-replay/` replay with no
mismatches.

## Level 3 board model and replay verification

Event 3 runs the `circusc4` trampoline handler documented in
`docs/LEVEL3_ROM_MODEL.md`: the two-column scroll with the `$F8` walk-in
page, the 8.8 rebound velocities (`$0420`/`$03C0` moving, `$FA65`
stationary) with the roof on the fourth stationary apex, the `$F517`
performer schedule, fire-breather flames and juggled knives with their
collision windows, the seven `$FA43` bags worth 300-900, the 20-point
landings, the 160-frame celebration with the bird and forty 100-point
coins for a perfect clear, the seven-frame hit, the fall, the 40-frame
fallen pose, the page step-back and 96-frame restart with the 4000/3500/
3000 bonus. `--replay capture.csv --replay-event 3 --replay-output
native.csv` (with `--replay-clear-projectiles` or `--replay-invulnerable`
for captures made that way) replays a `tools/autoplay_level3_headless.lua`
capture and `tools/compare_level3_replay.py` compares every board field
of every playing frame; the seven captures in
`docs/diagnostics/level3-replay/` replay with no mismatches.

## Event 4 board model and replay verification

Event 4 runs the `circusc4` rolling-ball handler documented in
`docs/LEVEL4_ROM_MODEL.md`: the one-column scroll with the `$F8` walk-in page,
the 57-frame jump arc, the `$9A42` landing window and the record swap that
makes the ball landed on the ridden one, the `$F1B5` spawn schedule that
tightens with every landing, the close-ball bonus worth 400 to 2000 points,
the 192-frame idle slip, the ball-versus-ball collision and the goal stand.
`tools/autoplay_level4_headless.lua` captures the board frame by frame and
`tools/compare_level4_replay.py` compares every column of every playing
frame; the five captures in `docs/diagnostics/level4-replay/` replay with no
mismatches.

## Vertical monitors

`--rotate 0|90|180|270` turns the whole presentation, and `F10` cycles it
while the game runs, which is easier when a cabinet monitor is being set up.
A monitor mounted in portrait needs no rotation — the game is 480x640 and
fills it. A landscape panel turned on its side wants `--rotate 90` (or `270`,
depending on which way it was turned) together with `--fullscreen`:

```sh
./build/circus_charlie_hd --rotate 90 --fullscreen
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
