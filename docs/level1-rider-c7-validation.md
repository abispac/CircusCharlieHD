# Level 1 HD rider/lion Run C7 validation

## Scope and registration

This pass validates only `run/run c 7.png` against accepted A2/B2 and directly against C5/C6. No artwork was modified, resized, cleaned, or integrated. Gameplay and renderer behavior remain untouched.

The same C3–C6 method was repeated: one least-squares similarity transform per pose based jointly on lion hip, lion shoulder, and rider seat/saddle, permitting only uniform scale, rotation, and translation. Extended paws/legs, tail, mane, and total silhouette width did not control registration.

| Pose | Dimensions | Scale | Rotation |
|---|---:|---:|---:|
| A2 | 1448×1086 | 0.449308 | −2.735° |
| B2 | 1448×1086 | 0.460818 | −1.407° |
| C5 | 1672×941 | 0.536960 | −6.074° |
| C6 | 1672×940 | 0.537692 | −5.306° |
| C7 | 1602×981 | 0.573445 | −6.003° |

These are diagnostic values only.

## Rider/seat

| Transition | Reference | ΔX | ΔY |
|---|---|---:|---:|
| A2 → B2 | seat/pelvis | −0.0251 | +0.0012 |
| B2 → C5 | seat/pelvis | +0.0864 | −0.0118 |
| B2 → C6 | seat/pelvis | +0.0759 | −0.0172 |
| B2 → C7 | seat/pelvis | **+0.0708** | **−0.0223** |
| B2 → C7 | Charlie torso | +0.1668 | −0.0416 |
| B2 → C7 | Charlie head | +0.2319 | −0.0749 |
| C7 → A2 | seat/pelvis | −0.0458 | +0.0211 |
| C7 → A2 | Charlie torso | −0.1379 | +0.0479 |
| C7 → A2 | Charlie head | −0.2044 | +0.0773 |

C7 reduces the B2→C seat displacement by 0.0051 versus C6 (6.7%) and by 0.0156 versus C5 (18.1%). That is measurable improvement, but +0.0708 remains a material forward translation of the pelvis relative to the stable A2/B2 saddle relationship. The torso/head, grip, and boots preserve the intended forward action lean; those are not required to align with A2/B2. The failure is the pelvis itself, not the lean.

**RIDER/SEAT: B) IMPROVED BUT STILL FORWARD.**

## Rigid lion torso

| Pose | Shoulder-to-hip | Excess vs 0.3564 |
|---|---:|---:|
| C5 | 0.3955 | 10.97% |
| C6 | 0.3995 | 12.09% |
| C7 | **0.4008** | **12.46%** |

C7 is 0.0013 longer than C6 and 0.0053 longer than C5. At this scale the C6→C7 change is small, but it is not an improvement and remains at the known long-torso level. Extended limbs, tail, mane, and overall width were excluded.

**LION TORSO: C) UNCHANGED** (still approximately 12.46% long).

## Preserved Run C structure

C7 preserves the fully extended front/rear legs, long tail, broad airborne silhouette, coherent paws, forward-leaning Charlie, general identity, and attached head/mane. No material regeneration damage is visible in those accepted features.

**PRESERVED RUN C STRUCTURE: A) PASS.**

## Animation continuity, head/mane, and depth

A2→B2 remains stable. B2→C7 has the correct concept—Charlie leans forward while the lion extends—but the remaining pelvis translation and long rigid torso still make the transition read partly as slide/stretch. C7→A2 would visibly reset those two structures on landing.

The C7 head, head base, and shoulder remain coherently connected. Natural airborne head attitude is acceptable and does not create a detached-head pop.

C7 normalized body depth is 0.2576, approximately 2.6% deeper than the A2/B2 mean of 0.2511. This is reasonable and passes.

## Original alpha

| Property | C7 |
|---|---:|
| Dimensions/mode | 1602×981 RGBA |
| Alpha range | 0–255 |
| Fully transparent | 59.91052% |
| Fully opaque | 0.00770% |
| Alpha > 0 bounds | `(0,0)–(1582,981)` |
| Alpha ≥ 224 bounds | `(66,12)–(1566,933)` |

| Alpha interval | Pixels |
|---|---:|
| 1–7 | 2.29587% |
| 8–15 | 0.12115% |
| 16–31 | 0.12943% |
| 32–63 | 0.15660% |
| 64–127 | 0.24192% |
| 128–223 | 0.45445% |
| 224–254 | 36.68236% |

The edge-touching alpha>0 bounds are remote low-alpha residue from the known export artifact; near-opaque subject bounds remain inset. Alpha is separate from geometry and was not cleaned.

## Final decision

**4. C7 IMPROVED — BOTH STILL REQUIRE CORRECTION.**

Exact required numbers:

- B2→C7 seat/pelvis: **ΔX +0.0708, ΔY −0.0223**.
- C7 shoulder-to-hip: **0.4008**.
- C7 torso difference from A2/B2 mean: **+12.46%**.

Minimum remaining artwork corrections:

1. Move only Charlie's pelvis/seat farther backward while retaining the current forward upper-body lean, grip, and boot relationship.
2. Shorten only the lion's rigid shoulder-to-hip span by approximately 11–12%. Preserve its limbs, tail, head/mane, paws, depth, and broad airborne silhouette.

## Diagnostics and stop point

- `docs/diagnostics/level1-rider-c7-validation/run-c7-focused-comparison-sheet.jpg`
- `docs/diagnostics/level1-rider-c7-validation/c5-c6-c7-direct-comparison.jpg`
- `docs/diagnostics/level1-rider-c7-validation/alpha-before-after.jpg`
- `docs/diagnostics/level1-rider-c7-validation/metrics.json`

No production assets were prepared and nothing was integrated.
