# Level 1 HD rider/lion candidate set 2 — revised validation

## Scope

This revision supersedes the earlier automatic rejection based on the ±4-pixel landmark tolerance. The arcade originals remain authoritative for animation state, selector timing, motion direction, broad pose, gameplay anchor and all gameplay behavior. The HD figures are evaluated as detailed reinterpretations, with HD-to-HD animation stability as the primary test.

Nothing was integrated. The source candidates were not modified. All normalization and alpha experiments were performed on temporary diagnostic copies only. Gameplay, renderer coordinates, physics, collision, jump timing, course logic, camera behavior and Events 2–4 remain unchanged.

## Normalization method

The source canvas sizes were ignored. Each candidate received one similarity transform—uniform scale, translation and a small rotation—computed from three rigid references:

- lion hip;
- lion shoulder;
- rider seat/saddle.

Those references were fitted as a group to their corresponding authoritative guide points. No independent outer-bounds fit, nonuniform stretch, limb fit or per-landmark correction was used.

| Pose | Diagnostic scale | Rotation |
|---|---:|---:|
| Run A 2 | 0.449308 | −2.735° |
| Run B 2 | 0.460818 | −1.407° |
| Run C 2 | 0.424689 | −5.184° |

These transforms are diagnostic only. They are not proposed runtime offsets.

## Inter-frame stability

Transition values below are normalized screen displacement after the common rigid-reference normalization. X is normalized by 768 pixels and Y by 512 pixels. Positive X moves right; positive Y moves down.

### A → B

| Reference | ΔX | ΔY |
|---|---:|---:|
| Shoulder | −0.0208 | −0.0182 |
| Hip | −0.0166 | +0.0013 |
| Lion head | −0.0088 | −0.0222 |
| Head base | −0.0200 | −0.0206 |
| Seat / Charlie pelvis | −0.0251 | +0.0012 |
| Charlie torso | −0.0290 | −0.0063 |
| Charlie head | −0.0275 | −0.0025 |

**Conclusion: A and B form a visually stable pair.** The small leftward progression across the torso, seat and Charlie reads as a natural gathered gait state rather than a teleport. Their lion head attachment, rider height and body mass remain coherent.

### B → C

| Reference | ΔX | ΔY |
|---|---:|---:|
| Shoulder | −0.0439 | +0.0503 |
| Hip | −0.0837 | −0.0312 |
| Lion head | −0.0860 | +0.0379 |
| Head base | −0.0757 | +0.0324 |
| Seat / Charlie pelvis | **+0.0964** | −0.0035 |
| Charlie torso | **+0.1213** | +0.0086 |
| Charlie head | **+0.1442** | +0.0103 |

### C → A

| Reference | ΔX | ΔY |
|---|---:|---:|
| Shoulder | +0.0647 | −0.0322 |
| Hip | +0.1004 | +0.0299 |
| Lion head | +0.0948 | −0.0157 |
| Head base | +0.0957 | −0.0118 |
| Seat / Charlie pelvis | **−0.0713** | +0.0023 |
| Charlie torso | **−0.0923** | −0.0023 |
| Charlie head | **−0.1167** | −0.0079 |

**Conclusion: Run C would visibly pop at takeoff and landing.** After aligning the rigid torso references, Charlie's seat is roughly 74 diagnostic pixels farther forward than B, his torso roughly 93 pixels farther forward and his head roughly 111 pixels farther forward. The reverse displacement is visible at landing. This is not merely acceptable limb motion.

## Lion scale and Run C silhouette

The long Run C outer silhouette is predominantly caused by the intentionally extended rear/front legs and nearly horizontal tail. Those moving extremities are appropriate to the arcade extended-stride state and must not be shortened merely to match the original bounding box.

The rigid-body measurements nevertheless show a smaller but real inconsistency:

| Pose | Shoulder-to-hip length | Approximate torso height |
|---|---:|---:|
| Run A 2 | 0.3574 | 0.2457 |
| Run B 2 | 0.3554 | 0.2565 |
| Run C 2 | **0.3896** | **0.1949** |

Run A/B torso length differs by only 0.6%. Run C is about 9.3% longer than their mean and presents about 22% less body depth. Some height change is legitimate foreshortening in an extended leap, but the combination of a longer, shallower torso and shifted head base makes C read as a stretched version of the animal rather than solely a limb extension.

The lion head also shifts backward relative to the fitted body at the B→C transition, while Charlie moves forward. That opposition makes the takeoff pop more conspicuous.

## Charlie/lion relationship

- **Run A 2 and Run B 2:** seat, pelvis, torso and head move together and stay convincingly attached to the saddle/mane. Their relationship is stable enough for animation.
- **Run C 2:** Charlie is materially farther forward. The seat/pelvis shift is visible even before considering head lean. The artwork therefore needs a rider/saddle relationship correction; a runtime anchor adjustment would move the lion too and cannot solve it.
- Charlie's forward lean in C is stylistically appropriate, but the entire rider should not slide forward with that lean.

## Arcade pose comparison

### Run A 2

- **Broad state:** correct planted/running pose.
- **Leg ordering:** acceptable interpretation; the wider leg spread is intentional moving-limb difference.
- **Torso/head attitude:** stable with B; acceptable stylistic anatomy.
- **Charlie lean/location:** coherent with B; more forward crouch than the pixels but acceptable.
- **Classification:** mostly **B** (stylistic reinterpretation) and **C** (moving limbs). No required stability correction.

### Run B 2

- **Broad state:** correct gathered/support pose.
- **Leg ordering:** correct gathered visual idea; paw placement differences are moving-limb choices.
- **Torso/head attitude:** nearly identical scale to A.
- **Charlie location:** stable with A.
- **Classification:** mostly **B/C**. No required stability correction.

### Run C 2

- **Broad state:** correct extended airborne direction.
- **Legs and tail:** the long silhouette is mainly intentional and belongs to **C**.
- **Torso:** moderately too long and too shallow relative to A/B; **A**, because the rigid animal changes shape.
- **Charlie:** substantially too far forward relative to the saddle/lion; **A**, because it creates a takeoff/landing pop.
- **Head attachment:** shifts relative to torso; **A** if retained after the torso correction.
- The gameplay trajectory, anchor and airborne hold are **D** and must remain untouched.

## Alpha analysis and non-destructive repair experiment

The alpha histogram strongly supports a uniform generation/export multiplier:

- approximately 27–31% of each entire source image is alpha 253;
- only 0.016–0.026% is alpha 255;
- the subject looks normal after continuously scaling alpha by `255/253`;
- legitimate intermediate antialias values remain intermediate rather than being thresholded to binary opacity.

The temporary diagnostic cleanup used two independent operations:

1. continuous alpha restoration `round(alpha × 255 / 253)`, clipped to 255;
2. removal only of alpha `<32` pixels that lie more than four pixels away from spatial support at alpha `>=32`.

The second operation preserves a four-pixel fringe around hair, mane, fur and other antialiased edges. It removes disconnected/remote atmospheric alpha without color-keying.

| Pose | Pixels changed by diagnostic alpha repair | Remote residue removed | Fully opaque afterward |
|---|---:|---:|---:|
| Run A 2 | 35.28293% | 0.25469% | 27.20537% |
| Run B 2 | 34.67283% | 0.26270% | 27.02922% |
| Run C 2 | 38.35652% | 0.32396% | 30.88991% |

**Conclusion: alpha can probably be cleaned safely and losslessly in appearance.** The continuous remap restores the intended opaque interior, and the spatial rule removes only remote low-alpha fog. Before production, the cleanup should be repeated by a checked-in deterministic tool and edge masks should be reviewed at 1:1 over light, dark and checkerboard backgrounds. The source files must remain untouched.

## Minimum corrections required before integration

1. **Keep Run A 2 and Run B 2 artwork geometry.** They already form a stable pair.
2. **Correct Run C's Charlie/saddle relationship in the artwork:** move the rider/seat relationship backward relative to the lion so pelvis, torso and head do not jump forward at takeoff. Preserve the intended forward body lean.
3. **Correct Run C's rigid lion torso only:** reduce the shoulder-to-hip stretch by approximately 8–10% and restore some body depth toward A/B. Do not shorten its extended legs or tail merely to reduce its bounding box.
4. **Recheck Run C head base after torso correction.** It should remain attached at the same scale and not jump backward when B switches to C.
5. **Apply the verified continuous alpha repair to production copies of all three images**, retaining the originals unchanged. Use spatially supported removal only for remote alpha `<32` residue; no binary threshold or color key.
6. **Do not correct any of this with gameplay:** no runtime X/Y offsets, separate airborne anchor, nonuniform runtime scale, physics changes, collision changes or trajectory changes.

## Decision

- **A/B stable:** yes.
- **C same character scale:** close stylistically, but not yet stable; its rigid torso and rider placement require correction.
- **Charlie seat changes noticeably:** A/B no; C yes.
- **Lion torso scale changes noticeably:** A/B no; C moderately yes.
- **Lion head jumps:** A/B minimally; C transition visibly shifts.
- **Moving limbs:** large paw, leg and tail changes are intentional and acceptable.
- **Alpha safely cleanable:** likely yes, using the tested continuous/spatial method—not a threshold.
- **Ready to integrate:** no. Minimum Run C geometry corrections and production alpha cleanup are required first.

## Diagnostics

- `docs/diagnostics/level1-rider-candidate-2-revised/revised-comparison-sheet.jpg`
- `docs/diagnostics/level1-rider-candidate-2-revised/alpha-before-after.jpg`
- `docs/diagnostics/level1-rider-candidate-2-revised/metrics.json`

The sheet contains the requested arcade/normalized/overlay rows, the three normalized HD poses, and A/B, B/C and C/A overlays with stable reference points. Metrics are retained separately so future corrected Run C artwork can be compared using the same method.

## Stop point

No production assets were created and nothing was integrated. This pass ends at the requested minimum-correction determination.
