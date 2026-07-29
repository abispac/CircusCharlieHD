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

## Stage 1 provisional values

These values are clean-room starting points and must be tuned from measured
play sessions:

- Ground speed: `112 units/second`
- Slow speed: `72 units/second`
- Fast speed: `166 units/second`
- Jump impulse: `-430 units/second`
- Gravity: `1080 units/second²`
- Ground height: `532 logical units`
- Lion collision box: intentionally smaller than the artwork

The debug overlay exposes cadence, velocity, jump height, camera position,
and output resolution. Later milestones will add frame-by-frame capture notes
and replayable input traces so physics changes can be compared objectively.
