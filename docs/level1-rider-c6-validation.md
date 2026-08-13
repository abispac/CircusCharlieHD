# Level 1 HD rider/lion Run C6 validation

## Scope and method

This pass validates only `run/run c 6.png` against accepted A2/B2 and directly against C5. No artwork was modified, cleaned, resized, or integrated. Gameplay and renderer behavior remain untouched.

The same C3–C5 registration was used: one least-squares similarity transform per pose based jointly on lion hip, lion shoulder, and rider seat/saddle, permitting only uniform scale, rotation, and translation. Tail, extended limbs, mane extent, and total silhouette width did not control registration.

| Pose | Source dimensions | Diagnostic scale | Rotation |
|---|---:|---:|---:|
| A2 | 1448×1086 | 0.449308 | −2.735° |
| B2 | 1448×1086 | 0.460818 | −1.407° |
| C5 | 1672×941 | 0.536960 | −6.074° |
| C6 | 1672×940 | 0.537692 | −5.306° |

These transforms are diagnostic only.

## Rider/seat

Displacements are measured after registration. X is normalized by 768 diagnostic pixels and Y by 512 pixels.

| Transition | Reference | ΔX | ΔY |
|---|---|---:|---:|
| A2 → B2 | seat/pelvis | −0.0251 | +0.0012 |
| B2 → C4 | seat/pelvis | +0.0555 | −0.0048 |
| B2 → C5 | seat/pelvis | +0.0864 | −0.0118 |
| B2 → C6 | seat/pelvis | **+0.0759** | **−0.0172** |
| B2 → C6 | Charlie torso | +0.1568 | −0.0268 |
| B2 → C6 | Charlie head | +0.2205 | −0.0444 |
| C6 → A2 | seat/pelvis | **−0.0508** | +0.0160 |
| C6 → A2 | Charlie torso | −0.1279 | +0.0331 |
| C6 → A2 | Charlie head | −0.1930 | +0.0469 |

C6 reduces C5's seat displacement by 0.0105 normalized X (12.2%), but remains 0.0204 worse than C4 and materially displaced from the stable A2/B2 saddle relationship. Torso/head movement is treated as legitimate forward lean, not independently rejected. Grip and boot remain coherent with that lean; the pelvis itself still slides forward enough to create a takeoff/landing saddle pop.

**RIDER/SEAT: B) FAIL.**

## Lion torso

| Pose | Shoulder-to-hip length | Excess versus A2/B2 mean |
|---|---:|---:|
| A2 | 0.3574 | — |
| B2 | 0.3554 | — |
| A2/B2 mean | 0.3564 | — |
| C5 | 0.3955 | 10.97% |
| C6 | **0.3995** | **12.09%** |

C6 is 0.0040 longer than C5 (approximately 1.01% relative) and therefore moves away from the stable A2/B2 torso span. Extended legs, tail, mane, and overall width were excluded.

**LION TORSO: D) REGRESSED.**

## Lion consistency and animation continuity

C6 preserves C5's successful characteristics:

- fully extended front and rear legs;
- long trailing tail;
- broad airborne silhouette;
- coherent head/mane attachment;
- recognizable same lion identity;
- reasonable paw anatomy and tail attachment.

Normalized body depth is 0.2310 versus C5's 0.2360 and A2/B2 mean 0.2511. The approximately 8.0% reduction from A2/B2 is acceptable airborne foreshortening and not a failure.

Head, head base, and shoulder remain anatomically connected. Natural head attitude changes do not create a detached-head effect.

A2→B2 remains stable. B2→C6 and C6→A2 retain the correct forward-leaning concept, but the independent pelvis displacement still reads as forward slide at takeoff and backward reset on landing.

## Alpha reference

| Property | C6 |
|---|---:|
| Dimensions/mode | 1672×940 RGBA |
| Alpha range | 0–255 |
| Fully transparent | 61.25808% |
| Fully opaque | 0.01425% |
| Alpha > 0 bounds | `(0,9)–(1658,940)` |
| Alpha ≥ 224 bounds | `(72,18)–(1647,904)` |

| Alpha interval | Pixels |
|---|---:|
| 1–7 | 1.30497% |
| 8–15 | 0.09270% |
| 16–31 | 0.10829% |
| 32–63 | 0.15041% |
| 64–127 | 0.23204% |
| 128–223 | 0.43845% |
| 224–254 | 36.40079% |

This is the known export-alpha artifact. It does not determine geometry, and no cleanup was performed.

## Required decision

**C6 RIDER/SEAT FAILED.**

- Exact B2→C6 seat displacement: **ΔX +0.0759, ΔY −0.0172**.
- C6 shoulder-to-hip measurement: **0.3995**, or **12.09%** longer than the A2/B2 mean.
- Lion torso classification: **D) REGRESSED** versus C5.

Minimum remaining artwork correction: move only Charlie's pelvis/seat backward while preserving the current upper-body lean, and shorten only the rigid lion shoulder-to-hip span by approximately 11–12%. Preserve the accepted limbs, tail, head/mane, and airborne silhouette.

## Diagnostics and stop point

- `docs/diagnostics/level1-rider-c6-validation/run-c6-focused-comparison-sheet.jpg`
- `docs/diagnostics/level1-rider-c6-validation/c5-c6-direct-comparison.jpg`
- `docs/diagnostics/level1-rider-c6-validation/alpha-before-after.jpg`
- `docs/diagnostics/level1-rider-c6-validation/metrics.json`

No production assets were prepared and nothing was integrated.
