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
- The first visual trace estimated a high arc, but hands-on testing showed that
  scaling it directly made the HD lion clear the ring center and strike the
  upper rim.
- The current HD collision-tuned arc reaches approximately `59 logical units`
  over about `76 frames`, placing the lion inside the fire-ring opening.
- The rider remains near the left `15–17%` of the playfield after scrolling
  begins.

Measured motion targets:

- Fast forward speed: `260 units/second`
- Backtracking is a real negative velocity, not merely slower forward motion.
- Jump impulse: `-188 units/second`
- Gravity: `300 units/second²`
- Ground height: `532 logical units`
- Lion collision box: intentionally smaller than the artwork

Current user-control model:

- Right accelerates toward `260 units/second`.
- Left accelerates toward `-150 units/second`.
- Releasing both directions decelerates to a stop.
- The single action button produces one repeatable arcade jump arc.
- Backtracking never turns Charlie or the lion around; they remain facing
  forward and move or jump in reverse.

This corrects the first prototype, where left only selected a slower positive
speed and therefore never let the player backtrack.

The debug overlay exposes cadence, velocity, jump height, and output
resolution. The replayable MAME input trace in `tools/capture_reference.lua`
writes temporary screenshots to `/tmp` so later tuning can be compared
objectively without committing copyrighted reference frames.
