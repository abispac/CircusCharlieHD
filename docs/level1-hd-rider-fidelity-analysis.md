# Level 1 HD rider/lion fidelity analysis

## Scope

This is an artwork and rendering analysis only. No artwork, physics, collision,
jump timing, hoop logic, course data, camera behavior, or Levels 2–4 were
changed.

Authoritative references:

- `reference/original/level1/rider/run-a.png`
- `reference/original/level1/rider/run-b.png`
- `reference/original/level1/rider/run-c.png`
- `reference/original/level1/rider/airborne.png`
- `reference/original/level1/metadata/assets.json`

The active HD source is `assets/stage1-rider-walk-12-v9.png`. Although
`stage1-rider-sheet-v8.png` is loaded, `drawLionAndRider()` always selects the
12-frame `riderWalkTest` texture when that texture loaded successfully. The
v8 sheet is therefore only a fallback in the current build.

## Diagnostic method and image legend

The analysis tool is `tools/analyze_level1_hd_rider.py`. It reads PNGs using
only the Python standard library and writes its measurements to
`docs/diagnostics/level1-hd-rider/metrics.json`.

The original tight PNG is restored to its invariant 48x32 upright hardware
composition envelope before alignment. The metadata's composition origin is
the upper-left of that envelope. For the gameplay comparison, its derived
ground anchor is the envelope's bottom center, `(24,32)`. The equivalent
native anchors are the atlas center plus the source ground line:

- grounded: `(256,348)`
- airborne in the current renderer: `(256,312)`

Every contact sheet has three columns:

1. authoritative arcade composite, nearest-neighbor enlarged;
2. closest reviewed HD frame at the authoritative gameplay anchor;
3. silhouette overlay at that same anchor.

In the overlay, cyan is the original, magenta is HD, white is overlap, and the
yellow cross is the anchor. The overlay is a pose/alignment diagnostic, not a
pixel-fidelity score: the HD art deliberately has much more silhouette detail
than a 1984 48x32 composite.

Generated diagnostics:

- `docs/diagnostics/level1-hd-rider/all-authoritative-poses.png`
- `docs/diagnostics/level1-hd-rider/run-a-contact-sheet.png`
- `docs/diagnostics/level1-hd-rider/run-b-contact-sheet.png`
- `docs/diagnostics/level1-hd-rider/run-c-contact-sheet.png`
- `docs/diagnostics/level1-hd-rider/airborne-contact-sheet.png`
- `docs/diagnostics/level1-hd-rider/current-native-airborne-render.png`
- `docs/diagnostics/level1-hd-rider/current-hd-12-frame-anchor-grid.png`

The combined sheet rows are Run A, Run B, Run C, and airborne, in that order.
The separate `current-native-airborne-render.png` intentionally shows what the
program renders now rather than the recommended match.

## Closest existing HD frames

| Original state | Closest reviewed HD frame | Reason |
|---|---:|---|
| Run A | 3 | Closest planted/galloping phase and best grounded silhouette score. |
| Run B | 8 | Closest compact support phase, with gathered legs under the torso. Raw silhouette scoring chose frame 3 by only 0.0003, but that ignores articulation and would collapse A and B into one pose. |
| Run C | 9 | Closest fully extended airborne-stride silhouette. |
| Airborne | 9 | The authoritative airborne art is Run C, so it must use the same HD art and anchor as Run C. |

The low absolute silhouette overlap values in `metrics.json` are expected:
the HD lion is proportioned differently and contains contours that cannot
match the sparse 48x32 original. They are used to rank candidates, not to
claim fidelity.

## Composition findings

### Charlie relative to the lion

Charlie is not reconstructed as a stable rider layer over a stable lion
skeleton. Every HD frame is a complete independently rendered composite.
Charlie's torso, head, hands, knee, and boot therefore shift with the lion even
when the arcade pose keeps the six hardware cells on one fixed 48x32
composition grid. The source normalizer locks a median dark-gold mane feature
to `(382,170)`; it does not lock Charlie's hands, seat, boot, or the lion's
skeletal landmarks. The result is small but visible rider drift against the
mane and saddle through the 12-frame cycle.

The arcade does change Charlie and the lion together between A, B, and C, but
those changes are three deliberate composite poses. It does not contain the
additional intermediate rider shifts present in the HD sequence.

### Lion dimensions and landmarks

The HD opaque bounds vary from 399 to 463 source pixels wide and from 293 to
333 pixels high. At the current 118.8-unit render width, that is a visible
width swing of about 15% and a height swing of about 14% between frames. This
is pose generation changing the perceived animal, not merely moving identical
limbs around a stable torso.

The head/muzzle, back line, hips, and tail base do not remain attached to a
consistent skeleton. The mane normalization reduces head-region drift, but it
cannot prevent changes in muzzle length, torso length, rump volume, or the
tail's attachment point. Frames 4 and 9 are especially long/low; frames 6 and
8 are especially compact.

### Paws and contact points

The original three poses share an invariant 48x32 composition envelope. In
the HD atlas, opaque bottoms range from source Y 312 to 348 even though the
grounded renderer assumes Y 348 for every grounded frame. Consequently the
visible body bottom sits between 0 and 36 source pixels above the nominal
ground line. At the current scale that is as much as 8.35 logical pixels of
vertical drift.

The bottom-contact X groups also move across unrelated parts of the image.
For example, frame 3 has principal bottom contacts around X 315–352 and
431–464, frame 8 around X 275–307, and frame 9 around X 471–503 plus a small
rear group. Some groups include tail or antialias pixels rather than paws,
which itself demonstrates why a single opaque-bottom constant is not a stable
foot anchor. Each selected pose needs explicit paw/contact anchors after its
art is corrected.

### Transparent padding and visual anchor

The atlas cells are uniformly 512x353, but their transparent margins are not:

- left padding: 49–111 pixels;
- right padding: 0–12 pixels;
- top padding: 5–21 pixels;
- bottom padding: 5–41 pixels.

The renderer anchors every frame at cell X 256, not at a verified body or
hardware-composite origin. Therefore changing transparent padding changes the
visible rider's screen position. The normalization target at a mane feature
does not make cell center a stable gameplay anchor.

## Animation-state findings

### Verified arcade sequence

The synchronized successful-hoop trace establishes this exact sequence:

| First MAME frame | State | Hardware sprite codes |
|---:|---|---|
| 1326 | Run A | `62 61 60 5F 5E 5D` |
| 1328 | Run B | `CD CC B6 B5 64 63` |
| 1336 | Run C | `5C 5B 5A 59 58 57` |
| 1343 | Run A | `62 61 60 5F 5E 5D` |
| 1347–1409 | Airborne, Run C art held | `5C 5B 5A 59 58 57` |
| 1410 | Landing, Run A | `62 61 60 5F 5E 5D` |
| 1418 | Run B | `CD CC B6 B5 64 63` |
| 1425 | Run C | `5C 5B 5A 59 58 57` |

Thus the arcade grounded cycle is `A -> B -> C -> A ...`. A successful jump
switches to C and holds C for the complete airborne interval. Landing selects
A. `airborne.png` and `run-c.png` have the same SHA-256 and metadata; absolute
hardware position creates the jump, not a special jump drawing.

### Current native sequence

The active native renderer does this instead:

- stationary and grounded: HD frame 1;
- moving and grounded: all 12 HD frames at 12 frames per second, selected from
  global `timeSeconds` rather than a three-state rider cadence;
- airborne: HD frame 4 held for the complete jump;
- landing: immediately returns to frame 1 only if stationary; while moving it
  returns to whichever global-time modulo-12 frame happens to be current.

Therefore native does not implement `A -> B -> C`, and a moving landing is not
guaranteed to select A.

### Extra jump motion introduced by rendering

The current jump uses a different artwork and a different internal anchor:

- current airborne artwork: HD frame 4;
- grounded source ground line: 348;
- airborne source ground line: 312.

The 36-source-pixel anchor change adds an 8.353-logical-pixel visual shift on
top of the verified jump trajectory. The original changes absolute composite
Y only; it does not change relative cell placement or use a different
composition anchor. Frame 4's distinct stretched pose and the special Y=312
anchor therefore introduce motion absent from the arcade.

Run and jump are rendered at the same destination width and height
(`118.8 x 81.90` logical units), so there is no state-specific scale factor.
The apparent size/position discontinuity comes from frame geometry and the
different source anchor. Texture sampling is linear for both states, which
does not change geometry but softens the HD asset during scaling.

## Recommendations

| Original state | Recommendation | Action in a later, explicitly approved asset pass |
|---|---|---|
| Run A | **MODIFY HD frame 3** | Preserve its general stride, but normalize lion torso dimensions, rider grip/seat, tail base, and explicit paw anchors to the authoritative A composite. |
| Run B | **REDRAW from the frame 8 concept** | Frame 8 is the nearest articulation, but its compact body and limb arrangement are too structurally different to repair reliably without redrawing that state against the verified B composite. |
| Run C | **MODIFY HD frame 9** | Preserve the extended stride, then normalize the lion skeleton, Charlie-to-lion placement, and authoritative composition/anchor. |
| Airborne | **MODIFY/REUSE the corrected Run C** | Do not create or retain separate airborne artwork. Reuse the corrected Run C pixels unchanged for the entire airborne state and let only gameplay position move them. |

No current HD state should be kept unchanged. The minimum faithful final atlas
is three corrected keyed composites with stable explicit anchors, not twelve
independently generated full-body poses. Any interpolation added later should
be derived between those keyed states without changing body proportions or
the arcade state sequence.

## Stop point

This pass ends at analysis and recommendations. No asset or gameplay
integration was performed.
