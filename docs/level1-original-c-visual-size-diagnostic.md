# Level 1 original Run C visual-size diagnostic

## Scope and source selection

This pass is diagnostic only. It does not replace production sprites or modify
renderer/gameplay behavior.

The compared masters are:

- Run A: `run/run a 2.png`, 1448x1086
- Run B: `run/Run b 2.png`, 1448x1086
- original pre-C2 Run C: `run/run c.png`, 1536x1024

The original Run C is identified independently by its filename, dimensions and
SHA-256 `51881d33b803c8dec3d6c3177db3767fd4e84642a6e4077aab583d6a677cc9f9`.
No C2-C8 candidate participates in this diagnostic.

## Method

A and B define the grounded identity-size target. The proposed independent
offline scales are fitted from multiple stable feature measurements:

- Charlie head, torso, boot/representative extremity scale;
- lion head, mane mass, vertical chest/body depth and representative paw;
- saddle width and height.

Total PNG size, total silhouette bounds, tail length, leg extension and total
pose width are excluded. The horizontal span of the extended Run C body is also
excluded from the scale fit; only its vertical body depth participates.

The proposed diagnostic scales are:

- Run A: `0.4220552832`
- Run B: `0.4380942671`
- original Run C: `0.4838703911`

These are proposed **offline master-to-production densities**, not runtime
scales. The renderer is unchanged.

Each preview uses zero rotation and translates a saddle/central-body reference
to the same diagnostic point `(512,438)`. Feet and extended limbs do not define
the anchor.

## Apparent feature sizes at the proposed scales

| Feature | Run A | Run B | Original Run C |
|---|---:|---:|---:|
| Charlie head | 221.6 x 126.6 | 227.8 x 124.9 | 183.9 x 137.9 |
| Charlie torso | 151.9 x 111.8 | 157.7 x 118.3 | 159.7 x 121.0 |
| Lion head | 217.4 x 185.7 | 225.6 x 188.4 | 212.9 x 176.6 |
| Mane envelope | 170.9 x 206.8 | 177.4 x 208.1 | 222.6 x 208.1 |
| Saddle | 111.8 x 90.7 | 111.7 x 89.8 | 145.2 x 101.6 |
| Lion body depth | 143.5 | 138.0 | 152.4 |
| Representative paw | 120.3 x 84.4 | 89.8 x 78.9 | 111.3 x 87.1 |

The most stable vertical/physical-size signals now group closely: Charlie torso,
lion head, mane height, body depth and paw height. Run C remains wider because
its pose is extended. Its distinct artwork has a narrower Charlie head and a
wider mane/saddle than A/B; those are intrinsic art differences and were not
"corrected" or hidden with renderer offsets.

## Alpha observation

The original Run C contains the historical dark/glow background as translucent
raster content. This diagnostic leaves those pixels untouched. No background
removal, alpha cleanup, color keying or painting was performed. Any later
production-preparation pass must address that separately and only after this
visual-size proposal is approved.

## Diagnostics

- `docs/diagnostics/level1-rider-original-c-size-study/original-a-b-c.jpg`
- `docs/diagnostics/level1-rider-original-c-size-study/proposed-normalized-a-b-c.jpg`
- `docs/diagnostics/level1-rider-original-c-size-study/proposed-anchor-overlays.jpg`
- `docs/diagnostics/level1-rider-original-c-size-study/simulated-a-b-c-airborne-c-a.jpg`
- `docs/diagnostics/level1-rider-original-c-size-study/measurements.json`

The previews preserve the original subject pixels except for the uniform
diagnostic resampling necessary to display the proposed production scale. No
production asset has been rebuilt or integrated.
