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
- Large hoops and the independently scheduled reserved small/prize-ring object
  continue moving along the overhead rail even when the lion is stopped or
  reversing.
- Three ordinary reusable slots (`$26d0/$2700/$2730`) and one reserved
  small/prize slot (`$2760`) are driven by the ROM course stream. The component
  cells at `$2400-$2490` are not child rings owned by ordinary hoops.
- Passing through a small ring consumes its randomized prize once but never
  stops, hides, or removes the moving ring itself.
- The original collision scan tests reserved object `$2760`. A centered upper
  crossing reaches its reward branch; the remaining inclusive Manhattan
  boundary is lethal. Native mirrors the instruction-level `$7130-$7192`
  point test rather than an HD-art rectangle.
- Stage 1 displays a short point popup at the object for every scored arcade
  action: `100` for a hoop crossing, `200` for a firepot, `500` or `1000` for
  a randomized moneybag, and `5000` for the hidden coin.
- The finish pose is presentation-only and renders at `134 x 141`, matching
  frame 003329 without changing the calibrated running sprite, collision body,
  or jump physics. Its paws rest on the green platform top.
- The single action button advances the measured, repeatable arcade jump
  table; holding the button does not alter height or duration.
- Backtracking never turns Charlie or the lion around; they remain facing
  forward and move or jump in reverse.
- A genuine airborne reverse crossing is accepted across the complete fixed
  jump arc. This makes the original backward-hoop secret reliable despite the
  shorter relative crossing interval when both the hoop and rider travel
  left. A LEFT + JUMP command also commits to reverse velocity on that same
  board sample, preventing a residual forward frame from causing a false
  collision. Walking backward into a hoop is still fatal, and forward
  collision is unchanged.
- The regular Stage 1 rider/lion sheet renders `10%` larger than the initial
  calibrated pass, around the same visual center and ground contact. Its
  measured collision body and jump table remain unchanged.
- Only the one secretly selected fire pot can reveal a coin, and only during
  a backward jump. Forward jumps and every other pot leave the secret dormant.

This corrects the first prototype, where left only selected a slower positive
speed and therefore never let the player backtrack.

The debug overlay exposes cadence, velocity, jump height, and output
resolution. The replayable MAME input trace in `tools/capture_reference.lua`
writes temporary screenshots to `/tmp` so later tuning can be compared
objectively without committing copyrighted reference frames.
