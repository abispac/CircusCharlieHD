# Physics baseline

The simulation is intentionally separated from rendering resolution.

## Timing

Official MAME hardware documentation describes:

- Pixel clock: `18.432 MHz / 3`
- Horizontal total: `384`
- Vertical total: `264`
- Simulation/display cadence: `6,144,000 / (384 × 264)`
- Result: `60.606060... Hz`
- Raw visible area: `256 × 224`
- Rotation: `90 degrees`

The prototype therefore advances gameplay at a fixed
`1 / 60.606060...` seconds. Rendering may happen at a different refresh rate
and interpolates visual positions between simulation steps.

## Stage 1 measured baseline

An automated reference trace was made locally from a legally supplied ROM.
The trace only records input timing and on-screen positions; no ROM graphics,
audio, or code are included in this project.

- The fast approach moves a ring by approximately `2 source pixels/frame`.
- At `480` logical pixels across, that is approximately `260 units/second`.
- The jump reaches its apex roughly `40 frames` after takeoff.
- A complete unobstructed jump lasts approximately `80 frames`, or `1.32 s`.
- The measured jump height is approximately `47 source pixels`, which maps to
  about `118 logical units` in the portrait playfield.
- The rider remains near the left `15–17%` of the playfield after scrolling
  begins.

Values now used by the Stage 1 prototype:

- Slow speed: `120 units/second`
- Cruise speed: `185 units/second`
- Fast speed: `260 units/second`
- Jump impulse: `-365 units/second`
- Gravity: `550 units/second²`
- Ground height: `532 logical units`
- Lion collision box: intentionally smaller than the artwork

The resulting jump has a calculated apex of `121 logical units` at
approximately `40.2 frames`, closely matching the observed baseline.

The debug overlay exposes cadence, velocity, jump height, and output
resolution. The replayable MAME input trace in `tools/capture_reference.lua`
writes temporary screenshots to `/tmp` so later tuning can be compared
objectively without committing copyrighted reference frames.
