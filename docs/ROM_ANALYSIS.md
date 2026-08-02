# Local ROM behavior analysis

The supplied ROM is used only as a clean-room behavioral reference. No ROM
bytes, decrypted program, graphics, sound, text, palette data, or temporary
screenshots are committed to this repository.

Both supplied arcade sets verify successfully in MAME:

- `circusc`: level-select version
- `circusc4`: no-level-select version

They share the same board driver but use different main program ROMs. Behavior
that differs between them will be measured separately rather than assumed.

## Region layout

MAME identifies the set as a Konami GX380 board:

- Main CPU: KONAMI-1 at `1.536 MHz`
- Program region: five `8 KiB` files mapped at `0x6000–0xffff`
- Audio CPU: Z80 with two `8 KiB` files
- Tiles: two `8 KiB` files
- Sprites: six `8 KiB` files
- Color/lookup PROMs: one `32-byte` and two `256-byte` files

The graphics files are not program code. They do not need to be guessed,
renamed, or concatenated into the native game.

## Decrypted program view

KONAMI-1 program opcodes are encrypted in the raw files. MAME already
implements the board’s decryption. Running `tools/dump_reference.lua` with
MAME’s debugger produces its decrypted 6809-compatible disassembly and memory
map under `/tmp`:

- `/tmp/circusc-maincpu.asm`
- `/tmp/circusc-memory-map.txt`

This is the useful equivalent of “descrambling” for behavior analysis. The
output is temporary and must not be added to Git.

## Confirmed input path

The decrypted disassembly currently confirms:

- `0x1001` is the player-one input port.
- The vertical-blank input latch at `0x647a–0x6488` reads and complements the
  system, player-one, and player-two ports.
- Current player-one input is stored in direct-page byte `$32`, physically
  RAM address `0x2032`.
- Gameplay routine `0x86d0` selects the active player, masks left/right bits
  `0x01–0x02`, and stores the horizontal state at `0x241b`.
- The same routine handles the jump input bits and debounce state through
  `0x241a`, `0x28f0`, and `0x28d3`.
- The sprite/object staging table spans `0x2400–0x27ff` in `0x10`-byte slots.
  Sprite screen coordinates are staged at offsets `+4` and `+6`, while the
  sprite code and attributes are staged at `+0e` and `+0f`.

## Visual trace findings

- MAME's board renderer draws background tile category `1`, then every sprite,
  then foreground tile category `0`. The Event 1 fire rings use the foreground
  tile pass rather than the ordinary sprite pass.
- In frame-by-frame `circusc4` captures the large hoop is only about `16`
  source pixels wide while the complete rider/lion composite is about `47`
  pixels wide. The whole narrow hoop crosses in front of a thin slice of the
  rider; its transparent center and rapid horizontal passage create the
  through-the-ring illusion. Splitting the hoop into artificial near/far arcs
  does not reproduce the arcade result.
- Rider anchor: approximately source `x = 34` on a `224`-pixel-wide upright
  image.
- The ceiling hoop uses a trolley/hanger immediately below the horizontal
  rail; it is not supported from the floor.
- Right input produces the fastest forward approach.
- Left input can return the rider toward the start and must be represented as
  real backtracking.
- In a player-driven Event 1 idle trace, the first ring moved approximately
  `123` upright source pixels over `240` frames while the lion remained at its
  start anchor. Fire-ring motion is therefore independent of lion movement.
- Jump behavior is edge-triggered in the input routine and uses a distinct
  airborne state in the actor structure.

## Deterministic autoplay capture

`tools/autoplay_reference.lua` records a frame-synchronized CSV trace of the
main inputs, gameplay state bytes, program counter, and every `0x10`-byte
sprite/object slot from `0x2400` through `0x27ff`. In `attract` mode it leaves
every control untouched so the original program's deterministic demonstration
is the driver and ground truth.
In `stage1_probe` mode it inserts a coin, selects the first event, then applies
a repeatable movement and jump schedule for controlled comparisons.
The `stage1_jump_tap` and `stage1_jump_hold` modes isolate one jump and support
configurable per-frame screenshots through `CIRCUS_SNAPSHOT_FIRST`,
`CIRCUS_SNAPSHOT_LAST`, and `CIRCUS_SNAPSHOT_EVERY`.
`stage1_crash` supplies a matched no-jump collision run. `stage2_idle` and
`stage2_crash` select the monkey event and provide matched stationary and
walk-into-monkey runs.

The capture also takes a native-resolution screenshot every 120 frames. MAME's
`-aviwrite` option can record the exact same run, keeping video, inputs, and RAM
on one shared frame counter.

The first complete `circusc` attract capture contains `6000` frames
(`99.0165 s`) at the original `224 × 256` upright output. It demonstrates the
fire-ring, tightrope-monkey, and trampoline events, including the title,
multicolor star wipes, event grid, ranking screen, crashes, and automatic
restarts. The fire-ring sprite sequence is active from captured frames
`1260–2364`.

The no-level-select `circusc4` set was traced through `4000` frames as a second
behavior reference. Its presentation timing differs: its first fire-ring
sequence becomes active earlier and remains active longer than in `circusc`.
The two sets will therefore remain separate deterministic fixtures.

During attract mode the physical input ports remain idle because the original
program drives its own internal demo. The trace still records all resulting
sprite slots and state transitions. `stage1_probe` supplies real active-low
coin, start, selection, direction, and jump inputs for controlled player-driven
comparisons.

## Private audio-reference method

MAME reports two `SN76489A` generators at `1,789,772 Hz`, an 8-bit R-2R DAC,
and a discrete-sound stage mixed to one mono output. Its `-wavwrite` option
records that mix at 48 kHz.

The same cold-boot input schedule is captured twice, changing only the
specific action under study. Phase-subtracting those matched recordings
removes most deterministic stage music and exposes the sound-effect envelope:

- `stage1_jump_tap` minus `stage1_crash` isolates the short descending jump
  tone before the no-jump run reaches the first collision.
- `stage2_crash` minus `stage2_idle` isolates the first monkey-contact effect.
- The later Event 1 difference window supplies the collision/burn reference.

ROM-derived WAV files remain outside this repository as private references.
Only timing, envelope, and pitch observations may guide newly synthesized
production audio.

## Why the effects cannot be copied out as individual WAV files

The two audio ROMs are ordinary Z80 program/data ROMs; unlike the encrypted
KONAMI-1 main program, they do not require opcode decryption. They do not hold
a folder of finished recordings. The main CPU writes a sound command at
`0x0800` and triggers the audio interrupt at `0x0c00`. The Z80 reads that
command at `0x6000`, then generates the audible result by programming two
SN76489A tone/noise chips, an 8-bit DAC, and the board's discrete filter/mixer.

Consequently, the faithful recovery workflow is command enumeration plus
MAME `-wavwrite` capture of the emulated final mono mixer. Each command must be
triggered in isolation, logged, recorded, trimmed, and matched to the action
that invokes it. This produces authentic private reference clips without
mistakenly treating arbitrary ROM byte ranges as PCM audio.

`tools/capture_sound_ids.lua` automates that command sweep. The clean isolated
sweep confirms hexadecimal command `0x41` (decimal `65`) as the coin sound.
The original game also uses it when the extra Charlie is collected; it is not
used for money bags or hidden fire-pot coins. Captured WAV output and command
logs remain private and are written outside the repository.

Auditioning the isolated command catalog confirms `0x49` (decimal `73`) as
the money-bag reward effect. The native game keeps money bags, the extra
Charlie, and hidden coins as independent event channels so later sound
assignments cannot affect unrelated rewards.

The deterministic `circusc4` attract trace also captures the complete hidden
fire-pot coin interaction. A reverse jump begins at frame `2775`; command
`0x50` launches the coin at frame `2838`, exactly 63 board frames later when
Charlie has landed beyond the pot. A separate forward jump begins at frame
`2886`, and a second `0x50` marks collection at frame `2904`. If it is not
collected, the coin completes its arc back into the pot without a second
sound. This timing is implemented as a pending launch after the reverse jump,
not as an immediate mid-jump pickup. The captured coin is near its apex about
50 frames after launch; the native 96-frame flight therefore reaches its peak
at frame 48 and passes Charlie on its descending half at the measured catch.
Its 151-logical-unit height is 35 percent taller than the initial prototype
arc, matching the more pronounced launch visible during gameplay review.

## Event 1 obstacle fairness invariant

The close double-hoop sequence remains intact, but its old `4260` floor-fire
slot is deliberately empty. Because the two hoops begin rail motion at
different activation times, the trailing hoop occupies that lane at common
play speeds and made the combined obstacle impossible. A runtime safety check
also makes any floor pot non-lethal while a moving hoop is within 76 logical
units, protecting slower and backtracking play without weakening ordinary
double hoops or ordinary fire-pot jumps.

## Recovered Event 1 rider labels

- Sprite slots `0x25f0–0x2640` form Charlie and the lion as a six-tile,
  `48 × 32` source-pixel composite.
- MAME's official driver identifies the graphics as packed-MSB four-bit
  `16 × 16` sprites. The six private sprite ROMs therefore contain 384
  individually addressable sprite cells.
- A grounded rider composite uses two columns by three rows before cabinet
  rotation. Confirmed animation groups include `57–5c`, `5d–62`, and the
  non-contiguous `63/64/b5/b6/cc/cd` group. These labels and silhouettes are
  used only as temporary clean-room proportion guides.
- `tools/decode_reference_sprites.py` can reconstruct temporary private
  blueprint sheets under `/tmp`; it never copies ROM pixels into production
  assets or the repository.
- Their vertical source coordinate produces a symmetric 64-sample jump table.
  The peak displacement is `55` source pixels and the complete takeoff to
  landing interval is `63` board frames.
- A two-frame tap and a held jump are identical, confirming a fixed jump.
- Grounded visible artwork occupies approximately `47 × 28` source pixels.
- In the controlled grounded frame, the rider floor contact is near source
  `y = 232`, the ceiling tube near `y = 140`, and the first ring spans
  approximately source `y = 152–216`. These relative coordinates drive the
  HD rail and ring placement.

## Recovered Event 2 animation labels

- A fresh `circusc` attract trace covers the complete tightrope sequence at
  object slots `$2400-$257f` while preserving the original `60.606 Hz` board
  cadence.
- Charlie's Event 2 jump is a separate fixed arc, not the Event 1 lion arc.
  It lasts 57 movement updates after takeoff, peaks at `52` upright source
  pixels, and returns to the rope on the 58th sampled pose.
- A standard monkey is a four-cell `32 x 32` composite. Its three ROM key
  poses are staged as `ce/cf/d0/fe`, `d1/d2/d3/fe`, and
  `ce/d5/d6/fe`. Their repeated hold pattern is approximately `7/5/7` board
  frames.
- The purple leap adds three distinct composites:
  `d7/d8/d9/fe`, `da/db/dc/fe`, and `ce/de/df/fe`.
- Production HD animation keeps those silhouettes and hold timing as the
  motion anchors, but adds one newly authored in-between pose between each
  ROM key pose. Standard walking, purple walking, and purple leaping therefore
  each render as smooth six-frame families without changing game speed or
  collision timing.
- The goal sequence stops Charlie on the raised green/red perch, awards the
  visible `5000`, and runs the FAROUT/GREAT crowd response before the shared
  bonus tally.

## Next labels to recover

- Camera threshold and backtracking limit
- Ring movement independent of camera movement
- Collision boxes for lion, hoop rim, fire pots, and collectible bags
- Bonus timer and distance-marker cadence
