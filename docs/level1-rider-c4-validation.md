# Level 1 HD rider/lion Run C4 validation

## Scope

This pass validates only `run/run c 4.png` against accepted references `run/run a 2.png` and `run/Run b 2.png`. Nothing was regenerated, resized, cleaned, or integrated. Gameplay and renderer sources were not changed.

The validation repeats the revised C3 methodology: one least-squares similarity transform per candidate using lion hip, lion shoulder, and rider seat/saddle. The transform permits only uniform scale, rotation, and translation. Outer silhouette width, tail, and extended limbs do not control registration.

| Pose | Source dimensions | Diagnostic scale | Rotation |
|---|---:|---:|---:|
| A2 | 1448×1086 | 0.449308 | −2.735° |
| B2 | 1448×1086 | 0.460818 | −1.407° |
| C4 | 1791×878 | 0.560818 | −3.985° |

The transforms are diagnostic only and are not runtime offsets.

## Check 1 — rider/seat continuity

Transition displacement is measured after rigid-reference normalization. X is normalized by 768 diagnostic pixels and Y by 512. Positive X is right and positive Y is down.

| Transition | Reference | ΔX | ΔY |
|---|---|---:|---:|
| A2 → B2 | seat/pelvis | −0.0251 | +0.0012 |
| A2 → B2 | Charlie torso | −0.0290 | −0.0063 |
| A2 → B2 | Charlie head | −0.0275 | −0.0025 |
| B2 → C4 | seat/pelvis | **+0.0555** | −0.0048 |
| B2 → C4 | Charlie torso | **+0.1209** | −0.0298 |
| B2 → C4 | Charlie head | **+0.1685** | −0.0656 |
| C4 → A2 | seat/pelvis | **−0.0305** | +0.0036 |
| C4 → A2 | Charlie torso | **−0.0919** | +0.0361 |
| C4 → A2 | Charlie head | **−0.1409** | +0.0681 |

Grip and boot remain internally coherent with Charlie's deliberately forward-leaning pose, but they do not change the pelvis result.

C4 improves the C3 B→C seat displacement from +0.0832 to +0.0555—about a one-third reduction. It does not fully eliminate the forward slide. The pelvis still advances independently of the lion's rigid registration, and the torso/head displacement magnifies the takeoff pop. Forward lean is valid; the remaining seat translation is not.

## Check 2 — rigid lion torso

| Pose | Shoulder-to-hip length | Approximate body depth |
|---|---:|---:|
| A2 | 0.3574 | 0.2457 |
| B2 | 0.3554 | 0.2565 |
| C4 | **0.4017** | **0.2465** |

The A2/B2 mean torso length is 0.3564. C4 is approximately **12.71% longer** than that mean. It therefore did not substantially eliminate C3's measured 11.25% elongation; under the same independent landmark method it is slightly longer.

Body depth is now excellent: C4 is only about 1.85% shallower than the A2/B2 mean and is entirely reasonable for the extended airborne pose.

Tail length and the front/rear leg extensions were excluded from the torso calculation and are not penalized.

## Check 3 — head/mane attachment

| Transition | Reference | ΔX | ΔY |
|---|---|---:|---:|
| B2 → C4 | shoulder | −0.0178 | +0.0376 |
| B2 → C4 | lion head | +0.0453 | +0.0172 |
| B2 → C4 | head base | −0.0053 | +0.0133 |
| C4 → A2 | shoulder | +0.0386 | −0.0194 |
| C4 → A2 | lion head | −0.0365 | +0.0050 |
| C4 → A2 | head base | +0.0253 | +0.0073 |

The mane/head base remains coherently attached to the shoulder. C4 does not regress the successful C3 improvement. The muzzle/head attitude changes naturally with the pose; there is no detached-head effect requiring an independent correction.

## Check 4 — extended-pose structure

C4 retains the intended broad Run C state:

- rear legs extend backward;
- front legs extend forward;
- tail trails fully behind;
- Charlie leans forward;
- the stretched silhouette remains coherent;
- no limbs appear compressed merely to imitate A2/B2's outer bounds.

The wider silhouette passes this check. It is not the reason for rejection.

## Check 5 — original alpha

| Property | C4 |
|---|---:|
| Dimensions | 1791×878 RGBA |
| Alpha range | 0–255 |
| Fully transparent | 60.51257% |
| Fully opaque | 0.00731% |
| Alpha > 0 bounds | `(48,13)–(1761,866)` |
| Alpha ≥ 224 bounds | `(56,17)–(1756,861)` |

Low-alpha distribution:

| Alpha interval | Pixels |
|---|---:|
| 1–7 | 1.92242% |
| 8–15 | 0.11905% |
| 16–31 | 0.13323% |
| 32–63 | 0.17673% |
| 64–127 | 0.26156% |
| 128–223 | 0.49088% |
| 224–254 | 36.37626% |

This is the same known image-generation alpha artifact. It remains a separate, deterministic production-cleanup concern and does not determine the geometry decision. C4 was not modified.

## Decision

**B) C4 still fails.**

Minimum remaining artwork correction:

1. Move Charlie's pelvis/seat slightly farther backward within C4—approximately the remaining normalized X difference of 0.055 relative to B2—while preserving the existing forward torso lean.
2. Shorten only the lion's rigid shoulder-to-hip span by approximately 11–13% toward the A2/B2 mean. Preserve the long tail and fully extended front/rear legs.

No separate head/mane correction is needed. Body depth and the extended limb structure are acceptable.

## Diagnostics

- `docs/diagnostics/level1-rider-c4-validation/run-c4-focused-comparison-sheet.jpg`
- `docs/diagnostics/level1-rider-c4-validation/alpha-before-after.jpg`
- `docs/diagnostics/level1-rider-c4-validation/metrics.json`

The focused sheet contains normalized A2, B2, C4, A2/B2 overlay, B2/C4 overlay, and C4/A2 overlay with hip, shoulder, seat/pelvis, Charlie torso/head, and lion head/head-base references.

## Stop point

No artwork or gameplay was changed or integrated.
