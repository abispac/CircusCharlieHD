# Level 1 large-hoop collision analysis

## Scope and trace

This pass changes only Level 1 large-hoop collision. It does not change jump
samples, hoop activation or position, course data, camera behavior, artwork,
fire-pot collision, bonus rings, or Levels 2–4.

`tools/trace_level1_large_hoop_collision.lua` reproduces the same attract-mode
input and instruction-traces logical frames 1383–1390. The debugger's `frame`
symbol describes the screen frame currently being processed, so an instruction
line labelled `F=1388` is the logic which produces the frame-1389 object
snapshot.

## Collision routine

The ordinary four-slot hoop loop begins at `$712D` with `U=$26D0` and advances
by `$30` at `$7195` through `$2760`. In the failing pass only `$26D0` is active;
its status is `01`, Y is `$9C`, X is 8.8 `$4C80`, sprite code is `$E4`, and
attribute is `$05`.

For each slot, `$7130-$7192` performs:

1. `$7130` reads only the object's X high byte at `U+$06`.
2. `$7132-$7137` computes `abs(objectX-$40)`.
3. `$7137-$7139` compares that distance with `$0E`; `BCC` rejects equality and
   greater values.
4. `$713B` preserves the horizontal distance on the stack.
5. `$713D` reads rider-composite Y from `$2644`.
6. `$7140-$7143` diverts the object referenced by direct-page pointer `<$BF`
   to the special collected/reward branch at `$71A0`; it cannot enter the
   failure path.
7. `$7145-$714B` adds `$10` to rider Y only for fourth slot `$2760`.
8. `$714D-$714F` subtracts `$B6`. A negative result is safe for ordinary
   slots. The `$2760` negative case instead runs the `$25E0` prize-state logic
   at `$7157-$718A`.
9. `$718C-$7190` adds the saved horizontal distance and compares the sum with
   `$1C`. `BHI` is safe; equality and lower values jump through `$7192` to
   failure routine `$7C47`.

Thus the ordinary large-hoop failure geometry is a single-point, inclusive
Manhattan boundary in source coordinates:

`abs(hoopX-$40) < $0E`

`riderY >= $B6`

`(riderY-$B6) + abs(hoopX-$40) <= $1C`

The object Y `$9C`, its X low byte, rendered hoop bounds, lion bounds, and
previous-frame coordinates do not participate. There are no explicit
direction or jump-state branches in this path; those states matter only
because the earlier movement/composite routines produce the X and Y bytes.

## Frames 1383–1390

The instruction trace reaches `$7130` for `$26D0` with these values. Values in
parentheses are the result at the indicated comparison.

| Logical frame | Rider `$2644` | Hoop X | `abs(X-$40)` | Result |
| --- | --- | --- | --- | --- |
| 1383 | `$B0` | `$58.80` | `$18` | rejected at `$7139` |
| 1384 | `$B3` | `$56.80` | `$16` | rejected at `$7139` |
| 1385 | `$B5` | `$54.80` | `$14` | rejected at `$7139` |
| 1386 | `$B7` | `$52.80` | `$12` | rejected at `$7139` |
| 1387 | `$BA` | `$50.80` | `$10` | rejected at `$7139` |
| 1388 | `$BD` | `$4E.80` | `$0E` | equality rejected by `BCC` |
| 1389 | `$C0` | `$4C.80` | `$0C` | Y delta `$0A`; sum `$16`; failure |
| 1390 | `$C0` | `$4C.80` | `$0C` | failure state already active |

At logical frame 1389, registers on entry are `U=$26D0`, `A=$4C` after the
object read, and `S=$2FF2`. `$713D` loads `A=$C0`; `$714D` produces `$0A`;
`$718C` produces `$16`; `$7192` jumps to `$7C47`. The first failure-pose write
is `$264E=$2F` at `$7C4A-$7C4D`.

## Execution order

Instruction order proves collision is evaluated against the already staged
object and rider bytes. `$7130-$7192` runs first. Later in the same tick the
jump/composite accumulator advances and the hoop scheduler and active-object
pass run at `$7607` and `$7539`. Sprite buffering presents the failure codes on
the following display frame. Native Level 1 now mirrors that order: it tests
the source-coordinate collision before its jump, scheduler, and hoop movement
updates, then enters failure without rolling back or modifying a jump sample.

## Native result and next divergence

The old HD rectangle/opening test has been bypassed for Level 1 large hoops.
Native derives the staged rider source row from the existing exact jump
sample and applies the constants `$40`, `$0E`, `$B6`, `$1C`, plus `$10` for
slot `$2760`.

The collision transition now synchronizes: native logic frame 67 aligns with
MAME logical frame 1389, and native frame 68 aligns with MAME's buffered
failure presentation at frame 1390. The focused comparator verifies the
frozen rider row, active failure state, and already-matched hoop position
through the end of the supplied MAME trace at frame 1410. No further
divergence exists in that trace window; the next divergence is beyond native
frame 88 / MAME frame 1410 and requires the next scoped trace to identify.
