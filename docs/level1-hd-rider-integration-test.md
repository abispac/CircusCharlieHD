# Level 1 HD rider/lion animation integration test

## Scope and result

This pass changes only Level 1 rider/lion artwork selection, animation-state
timing, and visual anchoring. It does not change movement, the jump table,
collision, hoop scheduling/position, course data, camera behavior, artwork
pixels, or Events 2–4.

The deterministic successful-large-hoop comparison is synchronized through
the complete comparison window. Native frame 25 is the first airborne sample,
native frame 88 is the landing transition, and the grounded arcade cadence
continues afterward without a state or anchor divergence.

## Original selector and cadence

The original grounded animation is not a fixed three-frame timer. Routine
`$73DC-$7405` reads the current course-position byte at `$2204`, compares it
with the previous sample in `<$B3`, and advances the selector in `<$B4` when
the modular displacement reaches the routine's `$12` threshold after its
`+$07` bias. Forward motion updates the stored sample by `$F5`; reverse motion
uses `$07`. `<$B4` advances modulo three.

At the synchronized full-forward-speed successful-hoop run, the observed
visible sequence is:

| Board/native frames | State | Hold |
|---|---:|---:|
| MAME 1322–1327 / native 0–5 | A | 6 samples in the captured window |
| MAME 1328–1335 / native 6–13 | B | 8 |
| MAME 1336–1342 / native 14–20 | C | 7 |
| MAME 1343–1346 / native 21–24 | A | 4 before takeoff |
| MAME 1347–1409 / native 25–87 | C airborne | 63 |
| MAME 1410–1417 / native 88–95 | A | 8 |
| MAME 1418–1424 / native 96–102 | B | 7 |
| MAME 1425–1431 / native 103–109 | C | 7 |

The first truncated A hold begins before the comparison window. Subsequent
holds alternate according to 8.8 course displacement, so the implementation
uses the selector mechanism rather than hard-coding `8/7/7` durations.

Takeoff routine `$7376-$7391` loads the Run C six-slot composite directly.
Landing routine `$7257-$727F` restores Run A, clears the selector, and reseeds
the prior-position sample from the current course byte.

## Artwork mapping

The temporary mapping requested for `stage1-rider-walk-12-v9.png` is active:

| Arcade state | HD atlas frame (human numbering) | Original sprite codes |
|---|---:|---|
| Run A | 3 | `62 61 60 5F 5E 5D` |
| Run B | 8 | `CD CC B6 B5 64 63` |
| Run C | 9 | `5C 5B 5A 59 58 57` |
| Airborne | 9 | `5C 5B 5A 59 58 57` |

The old selector cycled all twelve atlas cells from global elapsed time at 12
frames per second. It chose atlas frame 4 while airborne and resumed at an
arbitrary global-time frame on landing. The new selector uses only frames 3,
8, and 9 and follows the ROM state transitions above. Frame 9 remains unchanged
for all 63 airborne samples, then landing selects frame 3.

## Stable gameplay anchors

The authoritative arcade composite uses a stable `(24,32)` gameplay anchor
on its invariant `48x32` canvas. That normalized bottom-center anchor was
transferred to the measured opaque composite envelope of each selected HD
cell; atlas padding is excluded from placement.

| State / frame | Old source anchor | New source anchor |
|---|---:|---:|
| Grounded global frames | `(256,348)` | frame 3 / A: `(285,344)` |
| Grounded global frames | `(256,348)` | frame 8 / B: `(310.5,346)` |
| Grounded Run C | `(256,348)` | frame 9 / C: `(286,315)` |
| Airborne frame 4 | `(256,312)` | frame 9 / C: `(286,315)` |

Previously, changing from the grounded anchor to the special airborne anchor
introduced a 36-atlas-pixel visual shift in addition to the gameplay jump.
There is now no separate airborne anchor or visual Y correction. Grounded Run
C and airborne Run C use the same frame and the same `(286,315)` source anchor;
all vertical movement comes from the existing verified jump trajectory.

## Diagnostics

The executable accepts `--rider-diagnostic-dir` together with the existing
successful-hoop trace mode. It captures the pre-takeoff state, takeoff,
ascent, hoop crossing, final airborne sample, landing, and resumed Run B/C
states.

The corrected native sequence is shown in
[`corrected-native-hoop-jump.png`](diagnostics/level1-hd-rider-integration/corrected-native-hoop-jump.png).
Its frame descriptions are in
[`corrected-native-hoop-jump.json`](diagnostics/level1-hd-rider-integration/corrected-native-hoop-jump.json).
The frame-by-frame MAME/native state, gameplay, frame-selection, and anchor
comparison is in
[`state-comparison.csv`](diagnostics/level1-hd-rider-integration/state-comparison.csv).

The comparison checks rider source Y, hoop 8.8 X, collision result, landing,
score event, rider A/B/C state, selected HD frame, and selected HD anchor. It
reports no divergence through the synchronized comparison window.

## Remaining visual disagreement

Any remaining silhouette disagreement is in the existing HD pixels themselves,
not state selection or anchoring. In particular, the atlas frames still vary
in Charlie-to-lion placement, body proportions, paw silhouettes, and tail/body
shape as documented in the earlier fidelity analysis. This integration test
does not edit those pixels.
