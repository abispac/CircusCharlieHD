# Level 1 HD rider/lion Run C5 validation

## Scope and registration

This pass validates only `run/run c 5.png` against accepted references `run/run a 2.png` and `run/Run b 2.png`. No candidate was modified, cleaned, resized, or integrated. Gameplay and renderer sources remain untouched.

The C4 method was repeated: one least-squares similarity transform per pose based jointly on lion hip, lion shoulder, and rider seat/saddle. Only uniform scale, rotation, and translation were permitted. Extended limbs, tail, mane extent, and total silhouette width did not control registration.

| Pose | Source dimensions | Diagnostic scale | Rotation |
|---|---:|---:|---:|
| A2 | 1448×1086 | 0.449308 | −2.735° |
| B2 | 1448×1086 | 0.460818 | −1.407° |
| C5 | 1672×941 | 0.536960 | −6.074° |

These values are diagnostic only—not proposed runtime transforms.

## Check 1 — rider/seat attachment

Displacements are measured after rigid-reference normalization. X is normalized by 768 diagnostic pixels and Y by 512 pixels.

| Transition | Reference | ΔX | ΔY |
|---|---|---:|---:|
| A2 → B2 | seat/pelvis | −0.0251 | +0.0012 |
| A2 → B2 | Charlie torso | −0.0290 | −0.0063 |
| A2 → B2 | Charlie head | −0.0275 | −0.0025 |
| B2 → C5 | seat/pelvis | **+0.0864** | −0.0118 |
| B2 → C5 | Charlie torso | +0.1581 | −0.0262 |
| B2 → C5 | Charlie head | +0.2165 | −0.0478 |
| C5 → A2 | seat/pelvis | **−0.0613** | +0.0106 |
| C5 → A2 | Charlie torso | −0.1291 | +0.0325 |
| C5 → A2 | Charlie head | −0.1890 | +0.0503 |

The upper-body displacement is not itself treated as a failure because C5 correctly leans Charlie forward. The pelvis is judged separately. Its B2→C5 displacement is +0.0864, compared with C4's +0.0555. C5 therefore regresses the seat attachment rather than eliminating the C4 slide. Grip and boot remain coherent with the forward-leaning figure but do not correct the pelvis/saddle relationship.

## Check 2 — rigid lion torso

| Pose | Shoulder-to-hip length | Excess over A2/B2 mean |
|---|---:|---:|
| A2 | 0.3574 | — |
| B2 | 0.3554 | — |
| A2/B2 mean | 0.3564 | — |
| C3 | 0.3965 | 11.25% |
| C4 | 0.4017 | 12.71% |
| C5 | **0.3955** | **10.97%** |

C5 improves on C4 by approximately 1.54% in absolute torso length, but its rigid shoulder-to-hip span remains about 10.97% longer than the stable A2/B2 mean. This is not caused by the intentionally extended legs or tail; those were excluded from the measurement.

## Check 3 — body depth

| Pose | Approximate normalized body depth |
|---|---:|
| A2 | 0.2457 |
| B2 | 0.2565 |
| C4 | 0.2465 |
| C5 | **0.2360** |

C5 is approximately 6.0% shallower than the A2/B2 mean. That modest difference is acceptable natural deformation/foreshortening for the airborne stretched pose. It does not fail geometry.

## Check 4 — head/mane

| Transition | Reference | ΔX | ΔY |
|---|---|---:|---:|
| B2 → C5 | shoulder | −0.0360 | +0.0506 |
| B2 → C5 | lion head | +0.0086 | +0.0453 |
| B2 → C5 | head base | −0.0144 | +0.0336 |
| C5 → A2 | shoulder | +0.0568 | −0.0325 |
| C5 → A2 | lion head | +0.0003 | −0.0231 |
| C5 → A2 | head base | +0.0344 | −0.0130 |

The mane/head base remains attached coherently to the shoulder. Natural head attitude differs in the airborne pose, but there is no detached-head or severe head-pop failure. No separate head correction is required.

## Check 5 — extended Run C pose

C5 preserves all required Run C characteristics:

- extended rear legs;
- extended front legs;
- long trailing tail;
- broad airborne silhouette;
- forward-leaning Charlie;
- coherent overall lion anatomy.

The wide outer silhouette passes. It was not used as a rigid-torso proxy and is not a reason for rejection.

## Check 6 — original alpha

| Property | C5 |
|---|---:|
| Dimensions/mode | 1672×941 RGBA |
| Alpha range | 0–255 |
| Fully transparent | 63.11118% |
| Fully opaque | 0.02161% |
| Alpha > 0 bounds | `(0,15)–(1644,941)` |
| Alpha ≥ 224 bounds | `(85,27)–(1638,892)` |

| Alpha interval | Pixels |
|---|---:|
| 1–7 | 1.65735% |
| 8–15 | 0.10678% |
| 16–31 | 0.11879% |
| 32–63 | 0.15489% |
| 64–127 | 0.23148% |
| 128–223 | 0.42349% |
| 224–254 | 34.17443% |

The alpha-greater-than-zero bounds touching the left and bottom edges are caused by remote low-alpha residue, while the near-opaque subject remains inset. This matches the known export artifact and is reported separately from geometry. No cleanup was performed.

## Direct C4 versus C5

| Metric | C4 | C5 | Result |
|---|---:|---:|---|
| B2→C seat ΔX | +0.0555 | **+0.0864** | regressed by +0.0309 |
| Rigid torso length | 0.4017 | **0.3955** | improved by 0.0062 / 1.54% |
| Torso excess vs A2/B2 | 12.71% | **10.97%** | still materially long |
| Body depth | 0.2465 | **0.2360** | modestly shallower; acceptable |
| Head/mane attachment | pass | **pass** | retained |
| Extended pose | pass | **pass** | retained |

## Decision

**B) C5 GEOMETRY FAILS.**

Minimum remaining artwork corrections supported by the measurements:

1. Move only Charlie's pelvis/seat backward within the C5 composition so it remains attached to the A2/B2 saddle region. Preserve the current forward rotation/lean of his torso, head, arms, and grip.
2. Shorten only the lion's rigid shoulder-to-hip span by approximately 10–11% toward the A2/B2 mean. Preserve the extended legs, long tail, mane, and broad airborne silhouette.

No other stylistic or anatomical correction is requested. Body depth, head/mane attachment, and the extended pose are acceptable.

## Diagnostics and stop point

- `docs/diagnostics/level1-rider-c5-validation/run-c5-focused-comparison-sheet.jpg`
- `docs/diagnostics/level1-rider-c5-validation/alpha-before-after.jpg`
- `docs/diagnostics/level1-rider-c5-validation/metrics.json`

No production assets were prepared, and nothing was integrated.
