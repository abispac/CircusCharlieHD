# Level 1 HD rider/lion reconstruction specification

## Scope and authority

This document specifies three replacement HD rider/lion composites for Level 1. It does not change or reinterpret movement, jump trajectory or timing, hoop activation or position, collision, landing, scrolling, course data, the A/B/C selector, or the stable gameplay anchor. Those ROM-derived systems are the baseline.

The authoritative pose references are the verified 48×32 hardware composites:

- `reference/original/level1/rider/run-a.png`
- `reference/original/level1/rider/run-b.png`
- `reference/original/level1/rider/run-c.png`

Airborne is exactly Run C. It is not a fourth drawing and must not receive a different source anchor, vertical correction, scale, crop, or transparent padding.

Exact source silhouettes and reconstructed hardware pixels are authoritative. The anatomical landmark centerlines in this specification are careful visual interpretations of those pixels, generally to half-source-pixel precision; the arcade data does not label anatomy semantically. The complete machine-readable measurements are in [`measurements.json`](diagnostics/level1-hd-rider-reconstruction/measurements.json).

## Coordinate system

The original composite canvas is 48×32 source pixels. Its authoritative gameplay anchor is `(24,32)`, on the lower canvas boundary.

Normalized landmark coordinates are resolution-independent:

```text
normalized x = (source x - 24) / 48
normalized y = (source y - 32) / 32
```

Dimensions are normalized with `width / 48` and `height / 32`. Negative Y values are above the gameplay anchor. Rectangles use half-open bounds: left/top inclusive, right/bottom exclusive.

The recommended production canvas for every pose is 1024×768 transparent RGBA, with the gameplay anchor at `(512,640)`. This mapping preserves the measured structure at an integer scale:

```text
HD x = 512 + 16 × (source x - 24)
HD y = 640 + 16 × (source y - 32)
```

The original 48×32 composition envelope therefore maps to `(128,128)`–`(896,640)`. The 128-pixel margins above, left, right, and below the mapped envelope provide room for detailed fur and antialiased edges without changing the anchor or canvas between poses.

## Exact silhouette envelopes

| Pose | Exact source bounds | Anchor-relative normalized bounds | Recommended HD bounds | Source size | Normalized size |
|---|---:|---:|---:|---:|---:|
| Run A | `(0,0)`–`(47,32)` | `(-0.500,-1.000)`–`(0.479,0.000)` | `(128,128)`–`(880,640)` | 47×32 | 0.979×1.000 |
| Run B | `(0,0)`–`(45,32)` | `(-0.500,-1.000)`–`(0.438,0.000)` | `(128,128)`–`(848,640)` | 45×32 | 0.938×1.000 |
| Run C | `(0,0)`–`(46,29)` | `(-0.500,-1.000)`–`(0.458,-0.094)` | `(128,128)`–`(864,592)` | 46×29 | 0.958×0.906 |

Run C deliberately does not touch the anchor baseline. Its legs are extended forward and backward above that line. The existing gameplay trajectory supplies all airborne movement; artwork must not compensate for the raised paws.

## Shared structural rules

- Charlie and the lion form one inseparable composition. Charlie may not bob, scale, or slide independently relative to the saddle, mane, or lion.
- The lion's apparent torso mass and skeletal length remain consistent. Changes between A, B, and C come from gait, limb extension, head motion, tail motion, and rider posture—not a different-sized animal.
- Charlie remains seated over the lion's shoulder/back junction. His grip must remain visually attached to the mane in all poses.
- The lion always faces right. Its muzzle, front paws, and forward motion remain on the positive-X side.
- Transparent canvas, gameplay anchor, export scale, and color space must be identical among A, B, and C.
- The three images are discrete arcade states. Do not interpolate, morph, or create intermediate runtime frames.

## Run A template

Run A is the planted/contact phase. The lion is longest through the torso but its legs point predominantly downward.

| Landmark | Source `(x,y)` | Anchor-relative normalized | Recommended HD `(x,y)` |
|---|---:|---:|---:|
| Charlie head center | `(21,7)` | `(-0.0625,-0.7813)` | `(464,240)` |
| Charlie body center | `(20.5,16.5)` | `(-0.0729,-0.4844)` | `(456,392)` |
| Charlie seat | `(22.5,21.5)` | `(-0.0313,-0.3281)` | `(488,472)` |
| Charlie grip | `(28.5,13.5)` | `(0.0938,-0.5781)` | `(584,344)` |
| Charlie boot | `(25.5,22.5)` | `(0.0313,-0.2969)` | `(536,488)` |
| Lion head center | `(39,16.5)` | `(0.3125,-0.4844)` | `(752,392)` |
| Nose/muzzle | `(45,17.5)` | `(0.4375,-0.4531)` | `(848,408)` |
| Shoulder | `(32,17)` | `(0.1667,-0.4688)` | `(640,400)` |
| Hip | `(13.5,18)` | `(-0.2188,-0.4375)` | `(344,416)` |
| Tail base | `(10,17)` | `(-0.2917,-0.4688)` | `(288,400)` |
| Tail tip | `(0.5,24.5)` | `(-0.4896,-0.2344)` | `(136,520)` |

Charlie overall bounds are `(11,0)`–`(30,25)`; head `(11,0)`–`(30,14)`; body `(14,10)`–`(29,24)`. Lion overall bounds are `(0,9)`–`(47,32)`; torso `(9,13)`–`(35,25)`; head `(31,9)`–`(47,24)`.

Lion back line: `(9.5,14) → (16,13) → (24.5,14) → (31.5,13)`. Belly line: `(11.5,22) → (18,24) → (26.5,23) → (31,22)`.

Ground/contact paws are rear `(15,30)`, `(20,30)` and front `(30,31)`, `(35,31)`. Major limb vectors are `(+1.5,+10)`, `(+1.5,+8.5)`, `(+0.5,+10.5)`, and `(+2,+10.5)`. Preserve the visible holes between rear legs `(16,24)`–`(19,30)`, beneath the belly `(22,24)`–`(28,30)`, and between front legs `(31,24)`–`(34,30)`.

## Run B template

Run B is the gathered/support phase. The torso remains the same animal, while the paws tuck underneath it and the head shifts slightly upward and backward.

| Landmark | Source `(x,y)` | Anchor-relative normalized | Recommended HD `(x,y)` |
|---|---:|---:|---:|
| Charlie head center | `(20,6.5)` | `(-0.0833,-0.7969)` | `(448,232)` |
| Charlie body center | `(19.5,16)` | `(-0.0938,-0.5000)` | `(440,384)` |
| Charlie seat | `(21.5,21.5)` | `(-0.0521,-0.3281)` | `(472,472)` |
| Charlie grip | `(27.5,12.5)` | `(0.0729,-0.6094)` | `(568,328)` |
| Charlie boot | `(25,23)` | `(0.0208,-0.2813)` | `(528,496)` |
| Lion head center | `(38,15.5)` | `(0.2917,-0.5156)` | `(736,376)` |
| Nose/muzzle | `(43.5,16.5)` | `(0.4063,-0.4844)` | `(824,392)` |
| Shoulder | `(31,16.5)` | `(0.1458,-0.4844)` | `(624,392)` |
| Hip | `(12.5,18)` | `(-0.2396,-0.4375)` | `(328,416)` |
| Tail base | `(8.5,20)` | `(-0.3229,-0.3750)` | `(264,448)` |
| Tail tip | `(0.5,22)` | `(-0.4896,-0.3125)` | `(136,480)` |

Charlie overall bounds are `(10,0)`–`(29,25)`; head `(10,0)`–`(29,13)`; body `(15,9)`–`(28,24)`. Lion overall bounds are `(0,9)`–`(45,32)`; torso `(9,13)`–`(34,25)`; head `(30,9)`–`(45,23)`.

Lion back line: `(9,15) → (15,13.5) → (23,13.5) → (30.5,12.5)`. Belly line: `(10,22) → (17,24) → (25,23.5) → (30,22)`.

Ground/contact paws are rear `(10.5,30.5)`, `(17.5,30.5)` and front `(24.5,29.5)`, `(33,29.5)`. Major limb vectors are `(-1.5,+10.5)`, `(+0.5,+8.5)`, `(-3.5,+9)`, and `(+2,+9.5)`. Preserve negative spaces `(12,25)`–`(16,31)`, `(19,25)`–`(24,29)`, and `(27,24)`–`(32,29)`.

## Run C and airborne template

Run C is the extended-stride pose and the sole airborne image. The rear limbs extend left and the front limbs extend right. No separate airborne redraw, offset, scale, or anchor is permitted.

| Landmark | Source `(x,y)` | Anchor-relative normalized | Recommended HD `(x,y)` |
|---|---:|---:|---:|
| Charlie head center | `(18.5,7)` | `(-0.1146,-0.7813)` | `(424,240)` |
| Charlie body center | `(18.5,16.5)` | `(-0.1146,-0.4844)` | `(424,392)` |
| Charlie seat | `(20.5,21.5)` | `(-0.0729,-0.3281)` | `(456,472)` |
| Charlie grip | `(27,13)` | `(0.0625,-0.5938)` | `(560,336)` |
| Charlie boot | `(24,22.5)` | `(0.0000,-0.2969)` | `(512,488)` |
| Lion head center | `(38,16)` | `(0.2917,-0.5000)` | `(736,384)` |
| Nose/muzzle | `(44.5,17)` | `(0.4271,-0.4688)` | `(840,400)` |
| Shoulder | `(31,17)` | `(0.1458,-0.4688)` | `(624,400)` |
| Hip | `(12,18)` | `(-0.2500,-0.4375)` | `(320,416)` |
| Tail base | `(8,19)` | `(-0.3333,-0.4063)` | `(256,432)` |
| Tail tip | `(0.5,21)` | `(-0.4896,-0.3438)` | `(136,464)` |

Charlie overall bounds are `(8,0)`–`(28,25)`; head `(8,0)`–`(28,14)`; body `(11,10)`–`(28,24)`. Lion overall bounds are `(0,9)`–`(46,29)`; torso `(8,13)`–`(34,25)`; head `(29,9)`–`(46,24)`.

Lion back line: `(8,15.5) → (15,14) → (23.5,14) → (30,13.5)`. Belly line: `(9,22) → (18,24) → (27,23) → (31,21.5)`.

Airborne paw positions are rear `(2,26)`, `(7,27)` and front `(40.5,27)`, `(44,25)`, five to seven source pixels above the anchor baseline. Major limb vectors are `(-9,+6)`, `(-9,+5)`, `(+10.5,+6.5)`, and `(+12,+5.5)`. Preserve negative spaces `(4,23)`–`(8,27)`, `(20,24)`–`(28,27)`, and `(38,23)`–`(42,27)`.

## Charlie-to-lion relationship

These vectors are particularly important because the current HD source varies the rider/lion relationship between independently generated frames.

| Pose | Charlie head from lion head | Seat from shoulder | Grip from shoulder | Boot from shoulder |
|---|---:|---:|---:|---:|
| Run A | `(-18,-9.5)` | `(-9.5,+4.5)` | `(-3.5,-3.5)` | `(-6.5,+5.5)` |
| Run B | `(-18,-9)` | `(-9.5,+5)` | `(-3.5,-4)` | `(-6,+6.5)` |
| Run C | `(-19.5,-9)` | `(-10.5,+4.5)` | `(-4,-4)` | `(-7,+5.5)` |

The hip-to-shoulder vectors are approximately `(18.5,-1)`, `(18.5,-1.5)`, and `(19,-1)` for A, B, and C. This is the strongest constraint against the lion stretching or shrinking between poses. Tail-base-to-muzzle distance is 35, 35, and 36.5 source pixels; the slight C increase is limb/head projection in the stretched stride, not a scale change.

## Combined-image recommendation

Each pose should be reconstructed and exported as one combined Charlie/lion RGBA image. This gives the best chance of preserving the seat, grip, boot, mane overlap, and common anchor exactly. Independently generated Charlie and lion layers are likely to repeat the current drift in rider height, hand placement, saddle contact, and apparent scale.

Editable source files may contain separate internal layers for Charlie, saddle, mane, lion body, and limbs, but those layers must share one locked pose skeleton and be flattened into one runtime composite before export. Runtime composition of independently anchored Charlie and lion images is not recommended.

## Production dimensions and constraints

Use these same specifications for all three files:

| Property | Requirement |
|---|---|
| Canvas | 1024×768 pixels |
| Format | straight-alpha RGBA PNG, sRGB |
| Gameplay anchor | `(512,640)` |
| Geometry mapping | 16 HD pixels per authoritative source pixel |
| Background | fully transparent |
| Export crop | none; preserve the complete canvas |
| Runtime scale | identical for A, B, and C |
| Suggested names | `stage1-rider-run-a-hd.png`, `stage1-rider-run-b-hd.png`, `stage1-rider-run-c-hd.png` |
| Airborne source | reuse `stage1-rider-run-c-hd.png` exactly |

Generation or redrawing must obey these constraints:

1. Landmark joints, gameplay anchor, seat, grip, boot, paw endpoints, muzzle, tail base, and hip/shoulder centers must remain within ±4 HD pixels (¼ source pixel) of this template after final alignment.
2. Fur, hair, fabric, and antialiased silhouette detail may extend up to 8 HD pixels (½ source pixel) beyond a guide edge, but may not shift the measured structural centerline or crop the canvas.
3. Torso length and height must remain within 2% across A, B, and C. Do not resize a whole pose to make its paws meet the anchor.
4. Charlie's anatomy, costume scale, head scale, and seat height remain consistent across all states. His posture changes only as specified.
5. Keep the existing detailed HD visual language; do not imitate pixel art, but do preserve the original silhouette, proportions, overlaps, and negative spaces.
6. Do not add a separate shadow, dust, motion smear, flame, or background to these composites. Such effects would alter bounds and visual anchoring.
7. Do not create transition frames. The complete final set is Run A, Run B, and Run C; airborne uses the exact Run C pixels.

## Structural guides

The nearest-neighbor originals preserve every authoritative pixel without filtering:

- [`run-a-nearest-16x.png`](diagnostics/level1-hd-rider-reconstruction/run-a-nearest-16x.png)
- [`run-b-nearest-16x.png`](diagnostics/level1-hd-rider-reconstruction/run-b-nearest-16x.png)
- [`run-c-nearest-16x.png`](diagnostics/level1-hd-rider-reconstruction/run-c-nearest-16x.png)

The structural guides add a source-pixel grid and measured geometry:

- [`run-a-structural-guide.png`](diagnostics/level1-hd-rider-reconstruction/run-a-structural-guide.png)
- [`run-b-structural-guide.png`](diagnostics/level1-hd-rider-reconstruction/run-b-structural-guide.png)
- [`run-c-structural-guide.png`](diagnostics/level1-hd-rider-reconstruction/run-c-structural-guide.png)
- [`all-structural-guides.png`](diagnostics/level1-hd-rider-reconstruction/all-structural-guides.png)

Guide legend: white is the exact silhouette edge; cyan is Charlie; magenta is lion bounds and landmarks; green is the back, belly, and limb direction; purple marks important negative spaces; yellow is the authoritative `(24,32)` gameplay anchor.

The generator is [`tools/build_level1_hd_rider_reconstruction_spec.py`](../tools/build_level1_hd_rider_reconstruction_spec.py). It reads only the verified original composites and does not modify artwork or gameplay.

## Stop point

This pass ends with measurements, normalized templates, and visual guides. No current HD pixels, atlas data, gameplay selection, anchors, physics, collision, course data, or Levels 2–4 are modified.
