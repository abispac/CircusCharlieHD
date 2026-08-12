# Level 1 successful large-hoop trace

## Scope and deterministic input

This pass changes only Level 1 jump scheduling and successful-hoop score
commit order. It does not change the jump samples, collision constants, hoop
activation/position logic, course bytes, camera behavior, fire pots, bonus
rings, artwork, or Levels 2-4.

The deterministic `circusc4` run holds RIGHT from logical frame 1320 and holds
JUMP for frames 1346-1349. The recorded window is 1326-1430: 20 frames before
the input sample, the complete jump, and 20 frames after landing. Takeoff is
latched at 1346, `$20B0` becomes `01` at 1347, the first Y displacement appears
at 1348, and landing occurs at 1410.

The reproducible tools are:

- `tools/trace_level1_successful_large_hoop.lua`
- `tools/compare_level1_successful_large_hoop.py`
- native trace mode `successful-hoop`

Both traces self-terminate. The MAME trace records `$25F0-$2640`, all six
composite codes/attributes, `$2644/$2646`, `$20B0`, `$20B4-$20B6`, `$20B1`,
`$2203`, `$20C2`, `$2208`, `$20BC`, the nearest live hoop record, reconstructed
`$7130-$7192` result, BCD score, and landing transition.

## Original successful sequence

The first hoop is slot `$26D0`, Y `$9C`, code `$E4`. At frame 1346 its X is
`$A280`. It advances by `$FE00` each forward frame and reaches `$4080` at frame
1395 while Charlie is at source Y `$A8`. Collision remains `safe_above` through
frame 1401, then becomes `safe_x` as the hoop moves outside the horizontal
window. No failure branch is taken.

The jump accumulator is initialized to `$FC80` at frame 1347 and advances by
`$001C` per airborne frame, ending at `$0364`. Source Y is `$D0` before
takeoff, `$CC` on the first displaced frame, `$99` at the apex, `$CC` on the
last airborne frame, and `$D0` on landing.

Landing is the `$7257-$7270` transition: `$20B4` and `$20B0` are cleared and
the `$0100` award is queued through `$6114`; `$6250-$6279` commits it to
`$20A0-$20A2`. Consequently score changes on frame 1410, not at the visual hoop
crossing. The same frame activates the next hoop at `$2700` (`X=$FD80`) and
reloads the course accumulator to `$F400`, with course index `$02`.

## Exact rider/lion composite sequence

Every attribute byte is `$00`. Codes below are listed in slot order
`$25F0,$2600,$2610,$2620,$2630,$2640`.

| First frame | State | Codes |
|---:|---|---|
| 1326 | run A | `62 61 60 5F 5E 5D` |
| 1328 | run B | `CD CC B6 B5 64 63` |
| 1336 | run C | `5C 5B 5A 59 58 57` |
| 1343 | run A | `62 61 60 5F 5E 5D` |
| 1347 | airborne | `5C 5B 5A 59 58 57` |
| 1410 | landed/run A | `62 61 60 5F 5E 5D` |
| 1418 | run B | `CD CC B6 B5 64 63` |
| 1425 | run C | `5C 5B 5A 59 58 57` |

The three upper slots remain 16 source pixels below the lower-slot origin;
slot X values are `$45,$35,$25` for each row. This is a six-slot composite
transition, not an artwork offset or camera correction.

## Native synchronization and remaining divergence

Native frame 0 maps to MAME frame 1322. The native deterministic input is
therefore frame 24. Native now models the ROM's two-stage order explicitly:

1. input frame: queue the jump and retain grounded Y;
2. next frame: enter airborne state at displacement sample zero;
3. following frame: consume the first nonzero sample.

Hoop crossing now marks the hoop clear and queues 100 points; the landing
transition commits those points. The comparison checks rider source Y, hoop
8.8 X, collision result, landing, and hoop-specific scoring frame by frame.
It reports no divergence through MAME frame 1430/native frame 108, which is 20
frames after landing. The new first divergence is therefore beyond the
requested comparison window (`>1430`), not within this successful jump.

