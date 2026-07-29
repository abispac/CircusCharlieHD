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

- Rider anchor: approximately source `x = 34` on a `224`-pixel-wide upright
  image.
- The ceiling hoop uses a trolley/hanger immediately below the horizontal
  rail; it is not supported from the floor.
- Right input produces the fastest forward approach.
- Left input can return the rider toward the start and must be represented as
  real backtracking.
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

## Next labels to recover

The next trace pass will identify:

- Actor X/Y position and subpixel velocity
- Short-jump versus held-jump behavior
- Camera threshold and backtracking limit
- Ring movement independent of camera movement
- Collision boxes for lion, hoop rim, fire pots, and collectible bags
- Bonus timer and distance-marker cadence
