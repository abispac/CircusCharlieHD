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

- With the lion idle, the first fire ring travels approximately `123` source
  pixels over `240` frames, or about `0.51 source pixels/frame`.
- At `480` logical pixels across, the independent ceiling-rail motion is
  approximately `65 units/second`.
- Fast forward movement adds approximately `195 units/second`, preserving the
  combined `260 units/second` approach target.
- A controlled single-jump trace identifies a fixed 64-sample displacement
  table: 63 frame intervals from takeoff to landing, or approximately
  `1.0395 seconds`.
- The apex is exactly `55` upright source pixels above ground and lasts four
  samples. At the 640-pixel logical height this is `137.5 logical units`.
- A two-frame tap and a held button produce the same fixed jump. Difficulty
  must therefore be tuned through faithful obstacle timing and collision
  geometry, not by changing the jump arc.
- Charlie and the lion use a six-tile composite: `48 × 32` source pixels. The
  visible grounded pose is approximately `47 × 28` source pixels, which maps
  to approximately `101 × 70` pixels on the `480 × 640` logical canvas.
- A permanent `layout` capture scene places the MAME frame and remake on the
  same `480 × 640` grid. It calibrates the fixed marquee to `0–110`, the HUD
  to `110–202`, the crowd/fascia to `235–350`, the rail to `y = 350`, grass
  to `350–640`, and the rider's ground contact to `y = 590`. The decorative
  front-stage curtain is intentionally cropped out because it is absent from
  the Event 1 playfield.
- In the same grounded reference frame, the ceiling tube is approximately
  `92` source pixels above the rider's floor contact. The first ring spans
  roughly `80` to `16` source pixels above that contact, with its usable
  opening approximately `74` to `24` source pixels above it.
- The measured HD placement was raised after hands-on collision testing. The
  rail is at logical `y = 350`; the large-ring opening runs from logical
  `y = 367` to `y = 504`. This aligns the
  lion's collision body with the opening at the fixed jump apex. The long
  decorative pole in the HD source image is cropped out so the ring rides
  close beneath the tube like the arcade assembly.
- The rider remains near the left `15–17%` of the playfield after scrolling
  begins.

Measured motion targets:

- Fast forward speed: `195 units/second`
- Independent ring-rail speed: `65 units/second`
- Combined fast ring approach: `260 units/second`
- A ring begins its independent rail motion when its course section comes
  within `900` logical units of the camera. Once activated it never pauses.
  This preserves the late-stage sequence when the player waits or retries;
  distant rings can no longer travel past Charlie while still offscreen.
- Backtracking is a real negative velocity, not merely slower forward motion.
- Jump: exact 64-entry source-pixel displacement table sampled at board rate
- Peak displacement: `55 source pixels` / `137.5 logical units`
- Takeoff-to-landing time: `63 frames` / approximately `1.0395 seconds`
- Ground height: `590 logical units`
- Ceiling tube height: `350 logical units`
- Lion collision box: follows the lion's body rather than the transparent
  margins, mane, tail, or Charlie's upper-body artwork. Its horizontal center
  is offset `20` logical units from the sprite's world anchor, measured from
  the calibrated atlas frame, so rings and pots meet the visible lion body.
- Floor fire-pot collision uses a `32`-unit half-width and requires `42` units
  of vertical clearance, leaving a broad safe interval within the jump arc.
- Fire animation follows the original two-state cadence: the renderer swaps
  between slim and fuller flame silhouettes every `6` original frames. It does
  not layer procedural particles or free-moving flame triangles over the art.

Current user-control model:

- Right accelerates toward `195 units/second`; the incoming ring supplies the
  remaining `65 units/second` of relative approach.
- Left accelerates toward `-150 units/second`.
- Releasing both directions decelerates quickly to a stop. Reversing uses a
  stronger response so the rider brakes in roughly one board frame instead of
  sliding forward after LEFT is pressed.
- Large and small fire rings continue moving along the overhead rail even when
  the lion is stopped or reversing.
- Large rings use approximately `476` logical units of separation. A small
  ring follows the second large ring by approximately `493` logical units,
  matching the corresponding `222`- and `230`-source-pixel trace distances.
- The late double-ring test deliberately compresses the two centers to about
  `67` logical units after rail compensation, so the flames visually overlap
  like the recorded arcade sequence instead of reading as two separate tests.
- Passing through a small ring consumes its randomized prize once but never
  stops, hides, or removes the moving ring itself.
- Small rings use a `7`-logical-unit collision plane, thinner than the
  regular ring, plus a `98`-unit vertical safe opening. Their `112 x 198`
  target rectangle produces an approximately `50 x 110` visible oval because
  the source texture contains transparent horizontal padding. The
  visual preserves the broad oval surrounding the arcade money bag while the
  physical flame crossing remains narrow. Contact outside that
  opening while airborne burns both Charlie and the lion; running beneath the
  suspended ring is safe. A centered arcade jump passes and collects the
  randomized prize.
- The single action button advances the measured, repeatable arcade jump
  table; holding the button does not alter height or duration.
- Backtracking never turns Charlie or the lion around; they remain facing
  forward and move or jump in reverse.
- Only the one secretly selected fire pot can reveal a coin, and only during
  a backward jump. Forward jumps and every other pot leave the secret dormant.

This corrects the first prototype, where left only selected a slower positive
speed and therefore never let the player backtrack.

The debug overlay exposes cadence, velocity, jump height, and output
resolution. The replayable MAME input trace in `tools/capture_reference.lua`
writes temporary screenshots to `/tmp` so later tuning can be compared
objectively without committing copyrighted reference frames.
