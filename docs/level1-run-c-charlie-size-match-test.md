# Level 1 Run C Charlie-size production test

## Decision

**Visual runtime test: PASS, ready for user testing.**

Run A and Run B are byte-for-byte unchanged from commit `ffb0176`. Only the
offline production scale of original `run/run c.png` changed.

## Scale

- Run A: `0.4220552832` (unchanged)
- Run B: `0.4380942671` (unchanged)
- Run C: `0.44947802783109236`
- Previous Run C: `0.4838703911`
- Run C change: `-7.107763546%`

The C scale is the least-squares fit of Charlie's source head height and torso
height to the corresponding rendered A/B mean. Head width was not used because
Run C shows Charlie in a narrower side-profile/forward-lean pose. Lion legs,
tail and total silhouette never participate in the fit.

## Charlie measurements

Rendered production pixels are listed height x width:

| Pose | Charlie head H x W | Charlie torso H x W |
| --- | ---: | ---: |
| A | `126.62 x 221.58` | `111.85 x 151.94` |
| B | `124.86 x 227.81` | `118.29 x 157.71` |
| C | `128.10 x 170.80` | `112.37 x 148.33` |

C head and torso heights now match the A/B physical scale. Its narrower head
width is the intentional profile pose, not a production-scale reduction.

## Secondary lion measurements

Rendered pixels are width x height:

| Pose | Lion head | Mane | Body depth envelope | Representative paw | Saddle |
| --- | ---: | ---: | ---: | ---: | ---: |
| A | `217.36 x 185.70` | `170.93 x 206.81` | `211.03 x 143.50` | `120.29 x 84.41` | `111.84 x 90.74` |
| B | `225.62 x 188.38` | `177.43 x 208.09` | `214.67 x 138.00` | `89.81 x 78.86` | `111.71 x 89.81` |
| C | `197.77 x 164.06` | `206.76 x 193.28` | `301.15 x 141.59` | `103.38 x 80.91` | `134.84 x 94.39` |

The C body is intentionally wider because it is fully extended. Its body
depth and representative paw height remain compatible with A/B. No independent
lion scaling was applied.

## Actual runtime evidence

- `docs/diagnostics/level1-rider-production/actual-runtime-grounded-abca-cycles.jpg`
- `docs/diagnostics/level1-rider-production/actual-runtime-grounded-charlie-closeups.jpg`
- `docs/diagnostics/level1-rider-production/actual-runtime-rider-pixel-crops.jpg`

The actual grounded A -> B -> C -> A renderer sequence has no visible Charlie
height/torso-size pop at B -> C or C -> A. Grounded and airborne C use the same
production image and scale.

## Gameplay lock

The synchronized successful-large-hoop comparison matches all 120 rows. Player
position, jump state, hoop fixed-point X, collision result, landing, score,
animation state and logical gameplay anchor are unchanged. The only source
change outside production preparation is one additional diagnostic screenshot
frame at native frame 21 to capture the grounded C -> A transition.
