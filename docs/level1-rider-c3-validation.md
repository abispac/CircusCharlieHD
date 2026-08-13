# Level 1 HD rider/lion Run C3 validation

## Scope and method

This pass validates only `run/run c3.png`. `run/run a 2.png` and `run/Run b 2.png` remain the accepted comparison references. No candidate was regenerated, resized, cleaned, or integrated, and no gameplay or renderer source was changed.

The diagnostic repeats the revised candidate-2 method: one similarity transform per source image, using the lion hip, lion shoulder, and rider seat/saddle together as the rigid references. The transform permits only uniform scale, rotation, and translation. It does not use outer silhouette bounds, nonuniform scaling, independent limb fitting, or per-landmark correction. Measurements are visual landmark estimates and are normalized to the diagnostic screen, so source-canvas dimensions do not control the result.

| Pose | Source dimensions | Diagnostic scale | Rotation |
|---|---:|---:|---:|
| Run A 2 | 1448×1086 | 0.449308 | −2.735° |
| Run B 2 | 1448×1086 | 0.460818 | −1.407° |
| Run C3 | 1672×941 | 0.472520 | −4.482° |

These are diagnostic normalization values, not proposed runtime offsets.

## Alpha/transparency

Run C3 is RGBA with alpha range 0–255. Fully transparent pixels occupy 58.74560% of the source; fully opaque pixels occupy 0.00782%. The alpha-greater-than-zero bounds are `(0,13)–(1655,941)`, while the near-opaque alpha-at-least-224 bounds are `(28,19)–(1648,884)`.

The remote low-alpha haze and the interior alpha concentration match the already-observed export artifact. A temporary diagnostic copy was tested with the previously approved continuous `255/253` alpha restoration and spatially supported low-alpha cleanup. It removed 0.41408% remote residue without color-keying and changed 39.47413% of alpha samples through the continuous restoration. The source file was not changed. Alpha therefore does not reject C3 geometry and remains repairable later by the deterministic method.

## Charlie/saddle relationship

Transition displacement is measured after the rigid-reference normalization. X is normalized by 768 pixels and Y by 512 pixels.

| Transition | Reference | ΔX | ΔY |
|---|---|---:|---:|
| A2 → B2 | seat/pelvis | −0.0251 | +0.0012 |
| A2 → B2 | Charlie torso | −0.0290 | −0.0063 |
| A2 → B2 | Charlie head | −0.0275 | −0.0025 |
| B2 → C3 | seat/pelvis | **+0.0832** | −0.0072 |
| B2 → C3 | Charlie torso | **+0.1212** | −0.0091 |
| B2 → C3 | Charlie head | **+0.1511** | +0.0121 |
| C3 → A2 | seat/pelvis | **−0.0582** | +0.0060 |
| C3 → A2 | Charlie torso | **−0.0922** | +0.0154 |
| C3 → A2 | Charlie head | **−0.1236** | −0.0097 |

A2 and B2 keep Charlie attached to essentially the same saddle location. C3 preserves the intended forward lean, but the pelvis/seat also moves materially forward. Because the pelvis moves with the upper body, this reads as a rider slide rather than lean alone and would cause a visible B→C takeoff pop and C→A landing pop.

## Lion rigid torso and body depth

| Pose | Shoulder-to-hip length | Approximate body depth |
|---|---:|---:|
| Run A 2 | 0.3574 | 0.2457 |
| Run B 2 | 0.3554 | 0.2565 |
| Run C3 | **0.3965** | **0.2261** |

The A2/B2 mean rigid torso length is 0.3564. C3 is approximately **11.25% longer**, so it does not remove the earlier C2 torso-elongation problem. This measurement deliberately excludes the intentional long tail and extended front/rear legs; those features are not penalized.

C3 body depth is approximately 9.95% shallower than the A2/B2 mean. That is a substantial improvement over C2's roughly 22% reduction and is reasonably compatible with the extended pose and foreshortening. No pixel-identical anatomy is required. Body depth alone would not fail C3.

## Lion head attachment

| Transition | Reference | ΔX | ΔY |
|---|---|---:|---:|
| B2 → C3 | shoulder | −0.0340 | +0.0476 |
| B2 → C3 | lion head | −0.0359 | +0.0437 |
| B2 → C3 | head base | −0.0435 | +0.0402 |
| C3 → A2 | shoulder | +0.0548 | −0.0294 |
| C3 → A2 | lion head | +0.0448 | −0.0215 |
| C3 → A2 | head base | +0.0636 | −0.0197 |

Head, head base, and shoulder now travel together much more coherently than in C2. The prior large horizontal head pop is substantially reduced. A modest vertical shift remains, but it is compatible with the broad extended state and does not independently require a head redraw. The head/mane should simply be rechecked after the torso-length correction.

## Animation continuity and gameplay compatibility

- **A2 → B2:** visually stable and convincingly the same Charlie/lion.
- **B2 → C3:** the broad extended pose, long tail, and extended limbs read correctly, and head attachment is improved. The forward seat slide and longer rigid torso still make it look like an independently generated illustration at the transition.
- **C3 → A2:** the same two differences create a visible landing reset.

ROM-derived mapping, selector timing, airborne hold, trajectory, collision, hoop scheduling, and camera behavior remain authoritative and unchanged. Run C3 would be held unchanged while airborne. None of the remaining artwork disagreement can be repaired with a runtime offset, separate airborne anchor, gameplay change, nonuniform runtime scale, or per-pose renderer hack.

## Decision

**B) C3 still fails.**

Only these minimum artwork corrections remain:

1. Move Charlie's pelvis/seat backward within C3 so it remains attached near the A2/B2 saddle location. Preserve the forward lean; do not merely move the entire rider without correcting the seat relationship.
2. Shorten only the lion's rigid shoulder-to-hip torso by approximately 10–11% toward the A2/B2 body length. Preserve the intentionally extended legs and long tail.
3. After those two corrections, recheck that the existing improved mane/head attachment remains coherent. No independent head change is presently required unless that correction reintroduces a pop.

Alpha cleanup is not a geometry correction and can proceed later only after replacement geometry passes.

## Diagnostics

- `docs/diagnostics/level1-rider-c3-validation/run-c3-focused-comparison-sheet.jpg`
- `docs/diagnostics/level1-rider-c3-validation/alpha-before-after.jpg`
- `docs/diagnostics/level1-rider-c3-validation/metrics.json`

The focused sheet shows A2, B2, C3, the accepted A2/B2 reference overlay, B2/C3 overlay, and C3/A2 overlay with shoulder, hip, seat/pelvis, Charlie torso/head, and lion head/head-base references.

## Stop point

No production assets were prepared and nothing was integrated.
