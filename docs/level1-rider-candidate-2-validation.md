# Level 1 rider/lion replacement validation — candidate set 2

## Decision

**Validation failed. Stop before production preparation and integration.**

The three candidates have genuine RGBA transparency, but they do not satisfy the reconstruction specification. Almost all visible subject pixels are semi-transparent (`224–254`) rather than fully opaque, faint low-alpha residue expands materially beyond the near-opaque subject, and the pose geometry exceeds the permitted tolerances. Run C is the largest structural mismatch.

No candidate was resized, cleaned, color-keyed, segmented, integrated, or substituted into the renderer. No gameplay or ROM-derived state was changed.

## Alpha and bounds

Bounds are half-open `(left, top, right, bottom)`. The near-opaque subject uses alpha `>=224`.

| Pose | Source | Dimensions | Alpha | Fully transparent | Fully opaque | Alpha >=224 bounds | Alpha >0 bounds | Low-alpha expansion |
|---|---|---:|---:|---:|---:|---:|---:|---|
| Run A 2 | `run/run a 2.png` | 1448×1086 | 0–255 | 63.137763% | 0.025564% | `(22,67,1416,1009)` | `(17,48,1422,1062)` | 5 px left, 19 top, 6 right, 53 bottom |
| Run B 2 | `run/Run b 2.png` | 1448×1086 | 0–255 | 63.844968% | 0.018124% | `(15,67,1445,1018)` | `(0,19,1448,1060)` | 15 px left, 48 top, 3 right, 42 bottom |
| Run C 2 | `run/run c 2.png` | 1672×941 | 0–255 | 59.960962% | 0.016207% | `(46,33,1656,861)` | `(33,21,1660,917)` | 13 px left, 12 top, 4 right, 56 bottom |

The alpha histograms show the same defect in every file:

| Pose | Alpha 1–7 | 8–31 | 32–127 | 128–223 | 224–254 | 255 |
|---|---:|---:|---:|---:|---:|---:|
| A | 1.38268% | 0.25068% | 0.44057% | 0.49099% | 34.27176% | 0.02556% |
| B | 1.33600% | 0.22906% | 0.41601% | 0.45335% | 33.70248% | 0.01812% |
| C | 1.48600% | 0.29510% | 0.50758% | 0.53682% | 37.19733% | 0.01621% |

This is not merely a transparent preview background. The black/checkerboard area itself is transparent, but a faint generated alpha fringe/atmospheric residue is stored in the raster around the intended subject, particularly below all three poses and at the left edge of Run B. More importantly, the rendered figures themselves are nearly all alpha `224–254`; only about `0.02%` of each full image is truly opaque.

Cleaning this automatically would require distinguishing antialiased fur/hair edges from unwanted low-alpha residue and changing the alpha of the complete subject. A crude threshold or alpha remap would violate the edge-preservation requirement. Therefore no clean working copy was manufactured.

## Silhouette/aspect validation

| Pose | Candidate near-opaque aspect | Guide aspect | Deviation |
|---|---:|---:|---:|
| Run A 2 | 1.479830 | 1.468750 | **+0.7544% wider** |
| Run B 2 | 1.503680 | 1.406250 | **+6.9284% wider** |
| Run C 2 | 1.944444 | 1.586207 | **+22.5845% wider** |

Run A is close in overall envelope aspect but its limb endpoints and rider placement are not close. Run B is substantially too wide for its gathered phase. Run C is far too long and wide for the authoritative extended-stride silhouette.

## Normalized landmark deviations

The following are visual landmark-center estimates measured against the alpha `>=224` subject bounds and compared with the machine-readable specification. Values are `(candidate - guide)` in normalized subject coordinates; positive X is right and positive Y is down. Because anatomy is not machine-labelled in the generated raster, visual centers have an estimated measurement uncertainty of about ±0.015 normalized units. Errors far beyond that uncertainty remain conclusive.

| Landmark | Run A 2 Δ(x,y) | Run B 2 Δ(x,y) | Run C 2 Δ(x,y) |
|---|---:|---:|---:|
| Charlie head | `(-0.025,-0.040)` | `(-0.025,-0.037)` | **`(+0.308,-0.094)`** |
| Charlie body | `(-0.057,-0.152)` | `(-0.059,-0.160)` | **`(+0.246,-0.198)`** |
| Seat | `(-0.118,-0.196)` | `(-0.118,-0.217)` | **`(+0.166,-0.238)`** |
| Grip | `(-0.167,-0.052)` | `(-0.181,-0.046)` | **`(+0.155,-0.065)`** |
| Boot | `(-0.192,-0.074)` | `(-0.202,-0.111)` | `(+0.093,-0.079)` |
| Lion head | `(+0.026,-0.088)` | `(+0.005,-0.087)` | `(+0.052,-0.084)` |
| Muzzle | `(+0.017,-0.008)` | `(+0.009,-0.002)` | `(+0.010,-0.010)` |
| Shoulder | `(+0.028,-0.088)` | `(+0.007,-0.102)` | `(+0.105,-0.071)` |
| Hip | `(-0.009,+0.003)` | `(+0.012,-0.023)` | `(+0.083,-0.008)` |
| Tail base | `(-0.003,+0.019)` | `(+0.038,-0.091)` | **`(+0.108,-0.146)`** |
| Tail tip | `(+0.017,-0.110)` | `(+0.017,-0.106)` | **`(+0.023,-0.269)`** |

### Paw/contact deviations

| Paw/contact | Run A 2 Δ(x,y) | Run B 2 Δ(x,y) | Run C 2 Δ(x,y) |
|---|---:|---:|---:|
| Rear paw 1 | **`(-0.174,+0.016)`** | `(+0.043,-0.109)` | `(+0.024,-0.139)` |
| Rear paw 2 | `(-0.093,-0.074)` | `(-0.036,-0.004)` | `(-0.025,-0.071)` |
| Front paw 1 | **`(+0.171,-0.063)`** | `(+0.053,-0.051)` | `(+0.029,-0.065)` |
| Front paw 2 | **`(+0.179,-0.063)`** | `(+0.060,-0.030)` | `(+0.009,+0.022)` |

The specification permits only ±4 production pixels, which is ±0.0039 X and ±0.0052 Y on a 1024×768 canvas. The observed deviations are many times that allowance.

## Structural conclusions

### Charlie-to-lion relationship

- **Run A:** Charlie sits too high and left relative to the authoritative seat/shoulder relationship. Grip and boot are far left of their required points.
- **Run B:** the same rider drift remains, with the seat over 0.21 normalized height too high. The gathered lion pose is too wide.
- **Run C:** Charlie is dramatically too far right on the lion. The head-to-lion-head X relationship is approximately `-0.167` in the candidate versus `-0.424` in the guide. This alone disqualifies Run C.

### Torso length/height consistency

Using hip-to-shoulder landmarks, the candidate horizontal torso spans are approximately A `0.430`, B `0.406`, C `0.435`, versus guide spans A `0.394`, B `0.411`, C `0.413`. A and C are elongated, while the three generated poses do not maintain the required common torso skeleton within 2%. The apparent torso/shoulder height also shifts substantially because the mane and rider relationship change.

### Tail base and muzzle/head

- Muzzle endpoints are comparatively close in all three candidates.
- Lion heads and shoulders sit too high in all three.
- Run A tail base is close, but the tail tip is too high.
- Run B tail base and tip are too high.
- Run C tail base is too far right and high, with the tail tip about `0.269` normalized height too high.

### Anchor compatibility

- The candidates can technically be placed on a 1024×768 canvas, but they cannot share the authoritative `(512,640)` anchor and common runtime scale while preserving the required structure.
- Height-fitting A produces excessive forward/rear paw spread.
- Height-fitting B exceeds the authoritative width by 6.93%.
- Height-fitting C exceeds it by 22.58%, and its Run C feet/limbs do not occupy the authoritative raised-airborne relation to the anchor.
- Compensating with runtime offsets, a separate airborne anchor, nonuniform scaling, or gameplay-coordinate changes is prohibited and would not correct the internal rider/lion geometry.

## Required discrepancy categories

1. **artwork geometry — FAIL:** major landmark, paw, rider/lion, torso, and Run C silhouette errors.
2. **artwork transparency/edge quality — FAIL:** genuine alpha exists, but nearly the whole subject is semi-transparent and faint residue extends outside it.
3. **artwork anchor/scale — FAIL:** common authoritative anchor/scale cannot reconcile the three silhouettes.
4. **renderer integration — NOT TESTED:** integration was correctly blocked by validation.
5. **gameplay/ROM behavior — UNCHANGED:** no gameplay files or behavior were modified.

## Stop point

Phases 2–4 were not performed. No production PNGs were created, the old atlas remains the active rollback/reference source, and no deterministic gameplay run was necessary because no renderer integration occurred.
