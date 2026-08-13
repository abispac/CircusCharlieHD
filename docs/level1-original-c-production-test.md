# Level 1 original Run C production test

## Result

**Visual production test: PASS.**

The decision is based on actual native renderer captures, not solely on feature
measurements or synthetic sprite previews.

The production set is:

- A: `run/run a 2.png` at offline scale `0.4220552832`
- B: `run/Run b 2.png` at offline scale `0.4380942671`
- C and airborne: original `run/run c.png` at offline scale `0.4838703911`

No C2-C8 source participates in this production test.

## Production preparation

The supplied masters remain unmodified. The prepared copies are:

- `assets/stage1-rider-run-a-hd.png`
- `assets/stage1-rider-run-b-hd.png`
- `assets/stage1-rider-run-c-hd.png`

All are straight-alpha sRGB RGBA PNGs on identical 1024x768 canvases. The
offline transforms use zero rotation and independently apply only the approved
uniform scale and translation.

The stable saddle/central-body reference is placed at `(512,438)` in every
production canvas. The unchanged renderer gameplay anchor remains `(512,640)`,
so the common body-to-gameplay-anchor vector is `(0,202)` for A/B/C. Feet,
paws, tail and total bounds do not define the anchor.

## Original C alpha cleanup

The original C master contains a broad historical translucent glow/background.
The production copy uses a deterministic spatial cleanup:

1. restore continuous alpha with `round(alpha * 255 / 253)`, clipped to 255;
2. form the dense subject core from source alpha `>= 224`;
3. expand that core by five source pixels with an 11x11 maximum filter;
4. retain restored alpha only within that spatial subject support;
5. leave every RGB value untouched.

This removes distant glow/background pixels while retaining the connected
low-alpha edge fringe around fur, mane, tail hair, Charlie's hair and costume,
paws and whiskers. The output has no canvas-edge contact.

Inspection over white, black and checkerboard is recorded in:

`docs/diagnostics/level1-rider-production/alpha-light-dark-checkerboard.jpg`

The actual Level 1 renderer captures confirm that no dark/glow rectangle or
halo remains in gameplay.

## Runtime mapping

The pre-existing renderer mapping remains unchanged:

- state A -> production A
- state B -> production B
- state C -> production original C
- airborne -> the exact same production original C

The runtime renderer applies one identical source canvas, destination rectangle
and gameplay anchor to all three. There is no pose-specific runtime scale or
airborne correction.

## Gameplay lock verification

The real native executable was run with the synchronized successful-large-hoop
diagnostic. It produced 120 comparison rows against the accepted MAME trace;
all 120 matched.

It also matched the previously accepted native implementation for every frame
in these fields:

- player X/Y and velocity X/Y;
- grounded and jump-sample state;
- hoop screen X;
- landing transition;
- hoop score event;
- A/B/C selector state and selected HD asset;
- renderer anchor X/Y.

Airborne spans native frames 25-87 and always uses state C, asset 3 and anchor
`(512,640)`. Landing occurs at frame 88, returns to asset 1/Run A and awards the
same 100 points.

Machine-readable evidence:

- `docs/diagnostics/level1-rider-production/native-successful-large-hoop.csv`
- `docs/diagnostics/level1-rider-production/mame-native-state-comparison.csv`
- `docs/diagnostics/level1-rider-production/verification-summary.json`

## Visual decision from actual gameplay

Actual, unscaled renderer-pixel crops:

`docs/diagnostics/level1-rider-production/actual-runtime-rider-pixel-crops.jpg`

Full actual gameplay frames at their native 960x1280 backing-store size:

`docs/diagnostics/level1-rider-production/identity-scale-runtime-contact-sheet.jpg`

Observed result:

- A -> B reads as the same-sized lion gathering its stride;
- B -> C reads as the same-sized central character mass entering an extended
  horizontal leap, not as a medium-to-giant resize;
- Charlie, lion head and central body mass remain visually compatible;
- C's wider total bounds come from its intentional full extension;
- grounded C and airborne C have identical scale and artwork;
- landing returns to A without a visible character-size pop;
- the saddle differs intrinsically in the original C illustration but does not
  create an unacceptable runtime scale discontinuity;
- the shared central-body anchor does not visibly teleport;
- no original-C glow/background contamination is visible.

This pass changes only production copies and the deterministic preparation and
diagnostic tooling. No gameplay, physics, collision, camera, state timing,
scoring, hoop behavior, course data or Events 2-4 behavior was changed.
