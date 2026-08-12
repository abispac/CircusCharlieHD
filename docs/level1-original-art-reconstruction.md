# Level 1 original artwork reconstruction

## Scope and authority

This is an extraction/reference pass only. It does not replace the HD art or
change gameplay, collision, jump, hoop, camera, course, fire-pot, or Levels
2–4 behavior.

The authoritative inputs are the supplied `circusc4` ROM set, the official
MAME [`circusc.cpp`](https://github.com/mamedev/mame/blob/master/src/mame/konami/circusc.cpp)
driver, buffered hardware sprite RAM, and the synchronized successful Level 1
large-hoop trace. No pixels were generated, painted, filtered, or
antialiased.

## Graphics and palette decode

MAME declares the sprite region as `gfx_16x16x4_packed_msb`. Each 16×16 cell
uses 128 sequential bytes, with the high nibble representing the even X pixel
and the low nibble the odd X pixel. The six 0x2000-byte sprite ROMs are loaded
in board order from `380_j06.11e` through `380_j11.16e`, producing 384 cells.

The 32 indirect RGB colors come from `380_j18.2a`. The resistor-network
weights reproduced from the driver are `33/71/151` for red and green and
`81/174` for blue. `380_j16.10c` is the sprite lookup PROM. A source pen is
transparent when its lookup entry resolves to indirect color 0; transparency
is therefore not assumed to mean source pen 0.

The hardware sprite entry is four bytes: code, attributes, X, and Y. Attribute
bit `0x20` contributes code bit 8, the low nibble selects color, and bits
`0x40/0x80` flip X/Y. MAME draws ascending hardware entries from the VBLANK-
buffered bank. The board bitmap is finally rotated 90 degrees clockwise for
the upright cabinet display.

## Composite reconstruction

Charlie and the lion occupy hardware slots 31–36 in the synchronized run.
In board coordinates, the two composite rows use X 224 and 208; each row uses
Y 171, 187, and 203. After the cabinet rotation this becomes the familiar
three-by-two upright rider/lion composite. The exported files are tightly
alpha-cropped, while `metadata/assets.json` preserves the uncropped bounds,
component offsets, and anchor correction.

| Export | Sprite codes in slot order | Verified frame/state |
| --- | --- | --- |
| `rider/run-a.png` | `62 61 60 5F 5E 5D` | 1326 / grounded |
| `rider/run-b.png` | `CD CC B6 B5 64 63` | 1328 / grounded |
| `rider/run-c.png` | `5C 5B 5A 59 58 57` | 1336 / grounded |
| `rider/airborne.png` | `5C 5B 5A 59 58 57` | 1360 / airborne |
| `rider/death.png` | `38 37 36 35 30 2F` | failure trace 1390+ |

`airborne.png` is intentionally pixel-identical to `run-c.png`. The original
does not select different airborne cells, palette, flip flags, or relative
slot offsets. The jump changes the logical rider Y and consequently the
absolute hardware X after rotation; that state/position change makes the same
composite appear airborne.

The large fire hoop is hardware slots 45–47, using cells `E4 E5 E6` in a
single board-coordinate column separated by 16 pixels. Its logical template
records are `$26D0/$26E0/$26F0`, with source Y rows `$9C/$AC/$BC`. The three
visible animation states are color/attribute values `03`, `04`, and `05`,
exported as `large-hoop/hoop-00.png` through `hoop-02.png`.

A second, dedicated unmodified attract-mode capture maps two more Level 1
objects. The fire pot occupies slots 11–14 and is assembled from cells
`ED EF EC EE` as a two-by-two composite; its left column carries color
attribute `03`, while the right column uses color 0. The
small/bonus ring occupies slots 9–10 and uses cells `E9 EB`, deliberately
omitting the large hoop's middle `EA` cell. One independently zero-mismatch
frame of each object is exported. Other apparent palette phases were not
exported because overlap/priority prevented a zero-mismatch whole-composite
comparison.

The synchronized failure trace also identifies and exports the separate
three-cell left/right failure effects. These effects do not replace the six
rider records; they are later hardware slots layered with the death rider.

## Verification

`tools/capture_level1_original_art.lua` reproduces the successful hoop input,
self-terminates, captures MAME frames, and records both raw sprite-RAM banks.
`tools/capture_level1_unresolved_art.lua` performs the separate, self-
terminating unmodified attract-mode capture used for the unresolved-object
pass.
`tools/reconstruct_level1_original_art.py` decodes the ROM/PROM data, assembles
the component cells, rotates and crops them, writes metadata, and performs an
exact RGB comparison against unscaled 224×256 MAME screenshots.

The verification covers Run A, Run B, Run C, airborne, the large hoop, the
fire pot, and the small/bonus ring. Every reconstructed opaque pixel matched
the corresponding MAME frame:

- Run A: 731/731 pixels
- Run B: 731/731 pixels
- Run C: 714/714 pixels
- airborne: 692/692 pixels
- large hoop: 295/295 pixels
- fire pot: 412/412 pixels
- small/bonus ring: 145/145 pixels

The machine-readable report is `reference/original/level1/metadata/verification.json`.

## Exported assets and metadata

- `reference/original/level1/rider/`: four requested rider poses plus the
  verified death composite
- `reference/original/level1/large-hoop/`: three distinct palette animation
  states
- `reference/original/level1/fire-pot/`: one verified composite
- `reference/original/level1/bonus-ring/`: one verified composite
- `reference/original/level1/misc/`: two verified failure-effect composites
- `reference/original/level1/metadata/assets.json`: codes, ROM offsets/files,
  colors, attributes, flips, slots, component offsets, anchors, bounds,
  frames, records, and SHA-256 hashes
- `reference/original/level1/metadata/palette.json`: reconstructed palette and
  complete sprite lookup PROM
- `reference/original/level1/metadata/verification.json`: exact MAME pixel
  comparison

Every PNG is RGBA, nearest-neighbor source resolution, transparent, and
tightly cropped. No scaled derivatives are part of the reference set.

## Unresolved objects

The hidden coin and goal platform did not occur in the dedicated attract-mode
capture, so no hardware-backed composite could be established. The hanging
extra-Charlie candidate was identified as cells `41/42/43/44` in slots
0/1/45/46, but two of its opaque pixels are changed by an overlapping later
hardware slot in every observed frame. Because the requested whole-composite
test is therefore 266/268 rather than zero-mismatch, it was not exported.
Score/bonus object graphics also remain unmapped. `misc/UNRESOLVED.md`
records this boundary; no object was guessed or manually repaired.

`tools/capture_level1_manual_reference.lua` and
`docs/level1-manual-reference-capture.md` provide the read-only manual Level 1
capture pass for resolving these remaining objects. The capture preserves all
later hardware sprite slots so the hanging extra-Charlie candidate can be
verified with a hardware-derived priority/occlusion mask rather than rejected
because two genuine pixels are covered in the final framebuffer.
