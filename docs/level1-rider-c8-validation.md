# Level 1 HD rider/lion Run C8 validation

## Decision

**E) C8 passes geometry and is ready for alpha cleanup/integration.**

C8 passes the revised visual-anatomy and animation-continuity criterion. It is **not** an `OVERCORRECTED TORSO`. The projected shoulder-to-hip value remains longer than A2/B2, but C8 restores enough chest, abdomen, and pelvis mass that the lion reads as a normally proportioned lion in an extreme leap rather than a short torso attached to long limbs.

This is a diagnostic approval only. No source art was modified, cleaned, resized, or integrated, and no gameplay or renderer behavior was changed.

## Method

The test reused the revised C4–C7 method:

- one least-squares similarity transform per source image;
- registration landmarks: lion hip, lion shoulder, rider seat/saddle;
- permitted transform: uniform scale, rotation, translation only;
- no nonuniform scaling, independent limb fitting, outer-silhouette fitting, runtime offsets, or gameplay compensation.

Landmarks were independently measured on the unmodified source PNGs. Ratios are diagnostic evidence, not automatic pass/fail gates. The final decision gives priority to visual anatomy, saddle attachment, animation continuity, and the authoritative arcade Run C concept.

## C7 versus C8

After normalization, C8 changes C7 as follows:

| Feature | C7 | C8 | Finding |
|---|---:|---:|---|
| Shoulder-to-hip | 0.4008 | 0.4022 | Effectively unchanged (+0.35%); C8 was not made believable by crushing this span. |
| Body depth | 0.2576 | 0.2879 | Increased 11.76%; the central body has visibly more chest/abdomen/pelvis mass. |
| Torso length / depth | 2.3338 | 2.0953 | Strong improvement; C8 returns to the A2/B2 proportional band. |
| Torso length / head length | 1.8123 | 1.6997 | Closer to B2 (1.7148); the head no longer reads undersized relative to the body. |
| Torso length / representative extended-leg length | 1.1492 | 1.0485 | C8 emphasizes the arcade's extended leap. This does not indicate a miniature torso because body depth and mass increased. |

Specific visual changes:

- **Torso length:** no meaningful shortening. This is important: C8 avoided the failure mode of compressing shoulder and hip merely to satisfy the A/B number.
- **Torso volume:** improved. The abdomen and pelvis are fuller and the chest-to-hip bridge is continuous.
- **Chest:** fuller through the mane/shoulder transition, with no pinched neck-to-body join.
- **Abdomen:** deeper and less tubular than C7.
- **Hip:** fuller; rear legs emerge from a believable pelvic mass rather than from a narrow central block.
- **Shoulder:** stable relative to C7; front legs still originate from the chest/shoulder area.
- **Rear/front leg attachment:** coherent. C8 retains long extensions without moving their roots unnaturally close together.
- **Apparent leg length:** still deliberately long and broad, consistent with the original airborne concept.
- **Rider seat/pelvis:** shifted slightly rearward relative to C7 after normalization: `(-0.0116, -0.0059)`. This improves saddle continuity.
- **Charlie forward lean:** preserved. Most of the large forward displacement remains above the pelvis, so it reads as lean, not wholesale rider translation.
- **Head/mane:** remains attached. C8 slightly changes head attitude/scale but does not create a detached head pop.

## Visual anatomy sanity

C8 has none of the mandatory overcorrection symptoms:

- the torso is not miniature relative to the legs;
- shoulder and pelvis have not been pushed unnaturally close;
- front and rear legs do not emerge from the same compressed body region;
- the abdomen is not horizontally crushed;
- chest, abdomen, and pelvis retain substantial mass;
- the long tail and extended paws read as extremities of a broad leap, not as substitutes for missing body length.

Classification: **not overcorrected**.

## Rider and saddle continuity

Normalized transition displacements `(X, Y)`:

| Transition | Seat/pelvis | Charlie torso | Charlie head | Grip | Boot |
|---|---:|---:|---:|---:|---:|
| A2 → B2 | (-0.0251, +0.0012) | (-0.0290, -0.0063) | (-0.0275, -0.0025) | (-0.0306, -0.0080) | (-0.0204, +0.0050) |
| B2 → C8 | (+0.0592, -0.0282) | (+0.1575, -0.0597) | (+0.2278, -0.0981) | (+0.2089, -0.0193) | (+0.0768, -0.0029) |
| C8 → A2 | (-0.0341, +0.0270) | (-0.1285, +0.0660) | (-0.2003, +0.1005) | (-0.1783, +0.0272) | (-0.0565, -0.0021) |

The numerical B2→C8 pelvis displacement is visible but is not, by itself, a failure under the revised criterion. The source image shows Charlie's pelvis seated on the saddle pad; the much larger torso/head/grip displacement comes from intentional forward rotation and lean. Compared with C7, the seat moved rearward by 0.0116 normalized X while Charlie retained the intended forward posture. The saddle/rider relationship therefore reads as planted rather than sliding off the lion.

## Lion identity and animation continuity

### A2 → B2

This remains the strongest identity pair: similar torso mass, head/mane scale, saddle size, and rider placement with a normal gait change.

### B2 → C8

C8 changes into an extreme leap while retaining the same visual identity:

- mane and head remain comparable in scale;
- the neck/head attachment is continuous;
- saddle scale remains credible;
- chest, abdomen, and pelvis do not shrink;
- paws and legs extend dramatically, as required by Run C;
- Charlie's forward lean is an action change rather than a change of rider size.

### C8 → A2

The landing transition necessarily contracts the very broad Run C silhouette. The stable saddle attachment and retained central body mass prevent that contraction from looking like a different animal. No obvious torso-resize or head-detachment pop remains that should block production preparation.

## Torso and proportion evidence

| Pose | Shoulder→hip | Body depth | Length/depth | Length/head | Length/representative leg |
|---|---:|---:|---:|---:|---:|
| A2 | 0.3574 | 0.2457 | 2.1819 | 1.9346 | 1.4507 |
| B2 | 0.3554 | 0.2565 | 2.0782 | 1.7148 | 1.4589 |
| C7 | 0.4008 | 0.2576 | 2.3338 | 1.8123 | 1.1492 |
| C8 | 0.4022 | 0.2879 | 2.0953 | 1.6997 | 1.0485 |

The key result is not that C8's shoulder-to-hip value matches A/B—it does not. The key result is that its **length/depth ratio is 2.0953**, almost the same as B2's 2.0782 and within the A2/B2 visual identity range, while preserving the much longer extended legs. That is the expected signature of a substantial torso shown in a broad airborne pose.

## Arcade Run C concept

C8 preserves the authoritative concepts:

- elongated airborne lion;
- substantial central body mass;
- rear legs extended backward;
- front legs extended forward;
- long trailing tail;
- Charlie leaning forward while seated;
- natural head/mane attachment;
- broad horizontal leap.

It should not be shortened toward the A2/B2 projected shoulder-to-hip mean. Doing so would risk recreating the overcorrected short-body failure this revised test was designed to detect.

## Alpha observations

The C8 PNG is `1604×981` RGBA with alpha range `0–255`:

- fully transparent: `63.90834%`;
- fully opaque: `0.05707%`;
- alpha > 0 bounds: `(0, 3)–(1572, 981)`;
- alpha ≥ 224 bounds: `(98, 14)–(1562, 914)`;
- low-alpha 1–31: `27,201` pixels (`1.72867%`).

The alpha profile still contains the known generated-image fringe/residue and requires the already-approved deterministic continuous/spatial cleanup during the next production-preparation pass. It does not invalidate C8 geometry.

## Files

- `docs/diagnostics/level1-rider-c8-validation/run-c8-visual-continuity-sheet.jpg`
- `docs/diagnostics/level1-rider-c8-validation/metrics.json`

No artwork or gameplay files were changed.
