# Level 1 HD rider/lion production integration

## Scope

This pass prepares and integrates only the accepted Level 1 rider/lion set:

- Run A: `run/run a 2.png`
- Run B: `run/Run b 2.png`
- Run C and airborne: `run/run c 8.png`

The supplied masters remain unchanged. The old 12-frame atlas remains in the
repository as rollback/reference material, but normal Level 1 Run A/B/C and
airborne rendering no longer depends on it.

## Deterministic production preparation

`tools/prepare_stage1_rider_production.py` verifies the SHA-256 of each accepted
master before doing any work. It then creates production copies with:

1. continuous alpha restoration using `round(alpha * 255 / 253)`, clipped to
   255;
2. removal only of alpha values below 32 that fall outside the spatial support
   of the antialiased subject fringe (`alpha >= 32`, expanded by a deterministic
   9-by-9 maximum filter);
3. one similarity transform per pose based on the accepted hip, shoulder and
   seat landmarks;
4. one common 0.92 post-normalization scale about the shared gameplay anchor,
   used for all three poses to preserve accepted relative scale while keeping
   the complete extended C8 pose inside the canvas.

No binary threshold, color key, AI segmentation, manual painting or
pose-specific runtime correction is used.

The outputs are straight-alpha RGBA, embedded sRGB, 1024 by 768 pixels, and use
the common gameplay anchor `(512,640)`:

- `assets/stage1-rider-run-a-hd.png`
- `assets/stage1-rider-run-b-hd.png`
- `assets/stage1-rider-run-c-hd.png`

Exact source/output hashes, transforms, alpha statistics and opaque bounds are
recorded in
`docs/diagnostics/level1-rider-production/production-metadata.json`.

The light, dark and checkerboard inspection is in
`docs/diagnostics/level1-rider-production/alpha-light-dark-checkerboard.jpg`.
No canvas-edge contact or obvious detached residue remains.

## Renderer mapping

The renderer maps the existing ROM-derived state directly:

- A -> `stage1-rider-run-a-hd.png`
- B -> `stage1-rider-run-b-hd.png`
- C -> `stage1-rider-run-c-hd.png`
- airborne -> the exact same `stage1-rider-run-c-hd.png`

All states use the same `(512,640)` source anchor, production canvas and runtime
scale. There is no separate airborne image or anchor and no interpolation.
Vertical airborne movement comes only from the pre-existing verified jump
trajectory.

## Deterministic verification

The synchronized successful-large-hoop diagnostic compared 120 native frames
against the existing MAME trace. Every compared row matched for:

- rider source/logical Y;
- hoop 8.8 X;
- collision result;
- landing transition;
- score event;
- A/B/C rider state;
- selected production asset;
- gameplay anchor.

Key integration results:

- takeoff/airborne: native frames 25 through 87;
- every airborne frame selected state C, asset 3, anchor `(512,640)`;
- landing: frame 88 selected Run A, asset 1;
- hoop score event: 100 points on frame 88;
- the original A/B/C cadence resumed after landing;
- no gameplay/ROM divergence was introduced.

The machine-readable results are:

- `docs/diagnostics/level1-rider-production/native-successful-large-hoop.csv`
- `docs/diagnostics/level1-rider-production/mame-native-state-comparison.csv`
- `docs/diagnostics/level1-rider-production/verification-summary.json`

## Visual diagnostics

- Full native sequence:
  `docs/diagnostics/level1-rider-production/successful-large-hoop-native-contact-sheet.jpg`
- Original arcade A/B/C against final HD A/B/C at the shared anchor:
  `docs/diagnostics/level1-rider-production/original-vs-production-hd-anchor-comparison.jpg`
- Individual native captures:
  `docs/diagnostics/level1-rider-production/native-sequence/`

## Remaining issue classification

1. alpha/edge artifact: no material artifact visible on the three inspection
   backgrounds; fine antialiasing remains intentionally preserved;
2. artwork alignment: accepted reinterpretation differences remain visible,
   but the common gameplay anchor is stable and the extended C pose is not
   cropped;
3. renderer integration: none found in the deterministic sequence;
4. gameplay/ROM divergence: none through the 120-frame comparison window.

No other Level 1 artwork and no Events 2-4 behavior were changed in this pass.
