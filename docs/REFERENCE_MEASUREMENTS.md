# Clean-room reference measurements

The original arcade program is run locally only as a behavior reference.
This repository does not load, package, emulate, extract, or redistribute ROM
code, sprites, tile maps, palettes, sound, text, or logos.

## Board and display

- Raw board timing: `6,144,000 / (384 × 264) = 60.606060... Hz`
- Raw visible raster: `256 × 224`
- Cabinet orientation: `ROT90`
- Upright gameplay image: `224 × 256` source pixels
- Controls: two-way horizontal joystick and one action button

The native game uses a physically portrait `480 × 640` world and corrects the
original non-square-pixel presentation through direct vector/HD rendering.

## Event 1 trace

The reproducible trace:

1. Inserts a credit and starts player one.
2. Selects the first event.
3. Holds the fast direction.
4. Pulses the jump button at exact frame numbers.
5. Saves only temporary screenshots under `/tmp`.

Observed fast-motion sample:

- At frame `1620`, the first ring is near source `x = 188`.
- At frame `1640`, it is near source `x = 147`.
- At frame `1660`, it is near source `x = 106`.
- At frame `1680`, it is near source `x = 67`.

This is approximately `2 source pixels/frame`. The rider stays near source
`x = 34`, confirming a left-side camera anchor rather than a free-running
player centered on the screen.

Observed jump sample:

- Takeoff input: frame `1620`
- Rising pose: frame `1640`
- Apex region: frame `1660`
- Descent/collision region: frame `1680`

The first trace intentionally collides with the ring. That makes the ring rim,
opening, rider bounds, and failure timing visible in the same deterministic
sequence and gives the native collision model a repeatable test case.

## Visual construction notes

- The fire ring is suspended immediately below a horizontal ceiling track.
- Its hanger moves with the ring; the ring does not rise from a floor stand.
- Charlie is a clown with white face paint, red hair and nose, a bright coat,
  blue trousers, and a pointed performance cap.
- Event 1 keeps Charlie mounted on a lion.
- HD production assets must retain readable silhouettes at both 240p and HD.
