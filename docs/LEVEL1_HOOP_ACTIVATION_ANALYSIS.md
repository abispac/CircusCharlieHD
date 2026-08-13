# Level 1 hoop activation and position analysis

## Scope

This pass replaces only the authored Level 1 large-hoop intercept placement.
It does not change jump samples, collision geometry, artwork, fire-pot
positions, camera constants, bonus-ring placement, or Levels 2–4.

`tools/trace_level1_hoop_activation.lua` traces the four reusable object
records at `$26d0`, `$2700`, `$2730`, and `$2760`, their initialization and
movement writes, the activation accumulator, course index, scroll command,
and course-table reads. The successful late-jump run extends through the next
hoop activation so the result is not inferred from one object.

## Static object template

The setup code at `$6ee9-$7073` creates four three-cell hoop composites.

| Field | Object offset | Initial value | ROM code |
| --- | --- | --- | --- |
| Active/status | `+$00` | `00` | cleared during setup |
| Slot index | `+$03` | `00/10/20/30` | `$6fb1-$6fbb` |
| Primary Y | `+$04` | `9c` | `$6ee9-$6efb` |
| Secondary Y | `+$14`, `+$24` | `ac`, `bc` | `$6f03-$6f37` |
| X 8.8 | `+$06/+$07` | `0000` until activation | activation routine |
| Animation timer | `+$08` | `00` until activation | activation routine |
| Primary sprite code | `+$0e` | `e4` | `$6fdb-$6fe6` |
| Secondary sprite codes | `+$1e`, `+$2e` | `e5`, `e6` | template setup |

Attribute `+$0f` is animated rather than a one-time placement field.
`$755c-$7583` cycles the observed `03/04/05` values every three ticks. Thus
sprite code, Y, and slot identity come from the static template; activation
supplies status, timer, and object-local X.

## Activation routine and course data

The scheduler is `$7607-$767e`:

1. `$7607-$762f` derives the per-frame object delta from the current scroll
   command and subtracts the fixed rail term `$0080`.
2. It adds that signed 8.8 delta to `<$c2:$c3` (physical
   `$20c2:$20c3`). An unsigned underflow takes the activation path.
3. `$7655-$765a` indexes the course stream from base `$f7b4`. For this Level
   1 stream the first reads are `$f7c4=$dc`, then `$f7c5=$f4`.
4. `$7669-$766d` reloads the accumulator with the table byte as an 8.8 value
   (`dc00`, then `f400`) and `$766d-$7670` advances `$2208`.
5. `$7731-$7741` chooses the first inactive reusable record.
6. `$7742-$774a` sets active status, primes X/timer, and `$7679-$767e` writes
   the selected object's X to `$ff80`.

The Level 1 course reload bytes used by the ten large-hoop activations are:

`dc f4 d4 ec dc e4 de 1b ee f6`

The activation condition is therefore a course-table-driven timer/state
machine. It is not a world-coordinate lookup and is not derived directly from
the absolute scroll accumulator.

## Object-local movement

`$7539-$7554` updates each active hoop's own X 8.8 state by:

`object delta = current scroll command - $0080`

The observed cases are:

| Input | Scroll command | Hoop/activation delta |
| --- | --- | --- |
| none | `0000` | `ff80` (-0.5 source pixel) |
| forward | `fe80` | `fe00` (-2.0 source pixels) |
| backward | `0130` | `00b0` (+0.6875 source pixel) |

`$7586-$75e9` frees the object record after its X wraps past the left edge.
This is why the hardware needs only four records even though the course has
more hoops.

## Two activation proofs

The first activation occurs during MAME logical frame 1238:

- `$765a` reads `$f7c4=$dc`.
- `$7744` writes `$26d0+$00=01`.
- `$774a` writes timer `01`.
- `$767e` writes X `$ff80`.
- The ordinary active-object pass in that same frame advances it to `$ff00`.

The next activation occurs during logical frame 1409 and is visible in the
frame-1410 snapshot:

- the accumulator is `$0180` before the forward `$fe00` delta;
- `$765a` reads `$f7c5=$f4` and the course index becomes `02`;
- the first-free allocator chooses `$2700`;
- `$2700+$00=01`, timer `01`, sprite `e4`, Y `9c`;
- seed `$ff80` plus the same-frame forward update gives X `$fd80`;
- the activation accumulator is `$f400`.

This second allocation proves that the rule is general and slot-reusing.

## Native implementation

Native Level 1 now has:

- the four-record reusable hoop pool;
- the ROM course reload bytes and course index;
- the 8.8 activation accumulator;
- per-hoop object-local 8.8 X;
- first-free allocation and off-left retirement;
- the same input-dependent object delta and same-frame spawn update.

The first active object is initialized to the traced state immediately before
the native/MAME control alignment (`$26d0 X=$d480`, accumulator `$b180`). The
first native update therefore produces MAME frame 1322's `$d280` and `$af80`.
Large hoops no longer use `railStartForIntercept()`. A later verified pass
identified the reserved `$2760` object as the independently scheduled
small/prize ring and `$2400-$2490` as staged component cells; see
`docs/level1-remaining-rom-fidelity.md`.

## Earliest remaining divergence

At native frame 53 / MAME frame 1375, the formerly divergent hoop position
now matches: `$26d0 X=$6880` is `86.785714` logical units ahead of the rider
collision axis in both traces. It continues to match through native frame 63 /
MAME frame 1385.

The new first divergence is **native frame 64 / MAME frame 1386**. Native has
already entered `Scene::Crashed`, while MAME remains in play and does not enter
the `$7130-$7192 -> $7c47` failure transition until logical frame 1389
(presented at frame 1390). This is now a collision timing/geometry divergence,
not an activation or hoop-position divergence. It is reported but deliberately
not changed in this pass.
