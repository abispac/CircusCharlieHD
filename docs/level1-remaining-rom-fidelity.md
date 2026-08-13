# Level 1 remaining ROM-fidelity passes

This pass replaces the incorrect hoop/small-ring explanation in `e321034`
with the mechanism executed by `circusc4`. It preserves the accepted Level 1
movement, jump samples, large-hoop collision, rider A/B/C selector and art,
camera, landing, scoring, and Events 2–4.

## Regression audit of `e321034`

### Proven directly from ROM/MAME

- `$7344`, `$736C-$7391`, and `$7257-$727F`: airborne direction is latched at
  takeoff and live input resumes on landing.
- `$7539-$7554`: active hoop records move in independent 8.8 X state by
  `movement_command-$0080`.
- `$7607-$76F8`: the admission accumulator and course stream control new hoop
  objects.
- `$7130-$7192`: the large-hoop point/Manhattan collision path.
- Fixed sprite priority: the three hoop cells draw after the six rider/lion
  cells; transparent artwork creates the through-the-ring appearance.
- Start-line LEFT suppression while `$2203` is zero.

### Incorrect inference introduced by `e321034`

- `$2400/$2430/$2460/$2490` were treated as independently visible small
  flaming rings attached to each ordinary large hoop.
- Native consequently displayed a small ring for every large hoop and could
  construct combinations that were absent from the original course.
- Those component records were excluded from collision, making the displayed
  small flames harmless.
- Admission used a signed-result approximation instead of the two separate
  6809 conditions at `$7614` and `$762F`.

### Extra-Charlie-only additions

- A shared one-per-stage entitlement was added for the opening three reverse
  jumps and a backward crossing of a cleared hoop.
- The first version tried to reuse an already active future hoop. That was not
  the ROM behavior and could couple the reward to obstacle layout.

## Airborne direction locking

### Original mechanism and evidence

`$7344` reloads active movement command `<$B1:$B2` from the takeoff copy at
`$2243:$2244` while jump state `<$B0` is nonzero. `$736C-$7391` starts the
jump; `$7257-$727F` restores grounded control on landing. Direction is latched
at takeoff: opposite input or releasing the control is sampled but cannot
replace the airborne command. A neutral takeoff remains neutral.

`tools/trace_level1_airborne_direction.lua` captured right-held,
right-to-left, right-release, left-to-right, left-release, and neutral
takeoffs. Right jumps held `$FE80`, left jumps held `$0130`, and neutral jumps
held `$0000` through the airborne interval. Live input resumed on landing.

### Native correction and regression

`Player::level1AirborneDirection` stores the takeoff direction. Native uses it
only while airborne and restores live input when grounded. The deterministic
successful-large-hoop trace remains byte/field synchronized after the course
scheduler correction.

## Hoop visual priority and layering

### Original mechanism and evidence

The rider occupies hardware sprite slots 31–36. The three-cell large hoop
occupies slots 45–47. The board draws the complete hoop after the rider; there
is no crossing-time reassignment or priority-state transition. Transparent
pixels in the hoop center expose the rider while the flame rim remains in
front, producing the through-the-hoop illusion.

### Native correction and regression

Native stages the hanger/back presentation before the rider and draws the
transparent flame rim after the rider. This fixed ordering is unchanged by
the scheduler pass; large-hoop collision and rider artwork remain untouched.

## Course-start reverse movement

### Original mechanism and evidence

At the start line LEFT writes movement command `$0130`, but course scroll
`$2203:$2204` is still zero. `$7539` and `$7607` test high byte `$2203`; while
zero they replace the scroll contribution with `$0000`, then subtract the
intrinsic rail term `$0080`. Active objects therefore continue left by
`$FF80`, rather than moving right. After RIGHT establishes course progress,
normal reverse object movement returns: `$0130-$0080=$00B0`.

### Native correction and regression

`level1ForwardProgressed` represents that board boundary. Before it is set, a
LEFT command contributes zero to object movement/scheduling while player input
remains accepted; afterward LEFT contributes `$00B0`. This behavior is
preserved by the exact admission correction below.

## Exact large-hoop scheduler

`$7607` loads the signed movement command from `<$B1:$B2`. If course high byte
`$2203` is zero it substitutes zero; `$7611` subtracts `$0080`. A non-negative
result returns at `$76F9`. A negative result is added to activation accumulator
`<$C2:$C3` at `$762D`; a carry returns at `$76FB`. Admission occurs only for a
negative movement delta and an accumulator add with **no carry**.

On admission, `$7633-$765A` builds the course selector and reads the course byte
through table base `$F7B4`. Level 1 starts with `<$BB=$5E`. `$765A` adds that
offset and `$765C BITB #$03` selects the special boundary. Ordinary admission:

1. stores `course_byte << 8` in `<$C2:$C3`;
2. stores the byte in `<$BC`;
3. advances `$2208` even if the pool is full;
4. calls `$7731`, which searches only `$26D0`, `$2700`, `$2730`;
5. seeds an admitted ordinary slot at X `$FF80`.

The complete trace read these bytes from `$F7C4-$F7D0`:

`DC F4 D4 EC DC E4 DE 1B EE F6 1B DE EE`

Native now uses all thirteen observed bytes, initializes the selector offset to
`$5E`, advances the stream when the pool is full, and implements the exact
negative-delta/no-carry admission condition.

## Exact small-ring scheduler

The real small/prize ring is logical object `$2760`, not the staged component
cells at `$2400-$2490`. At a selector boundary:

- if prior course state `<$BC >= $60`, `$7664` branches to `$76CA`;
- `$2760` status and movement state become active;
- X is initialized to `$FF80`;
- on normal difficulty its next activation value is
  `(free_running_frame_byte <$14 | $80) << 8`;
- `$25E0=$FF` and `$25EE:$25EF=$FC00` initialize its prize presentation;
- course index `$2208` advances independently of reward entitlement.

This is its own course-stream opcode/state transition. It is not a child of a
large hoop, is not spawned by extra-Charlie state, and is not manually placed
relative to a future hoop.

## Complete chronological Level 1 hoop sequence

The complete attract-mode run was captured from the first Level 1 admission
through the final usable course event. `docs/diagnostics/level1-course/` holds
the per-frame snapshots, reduced events, and comparison output.

| MAME frame | stream index | table address/value | admitted object | active hoop set after admission |
|---:|---:|---|---|---|
| 943 | 0 | `$F7C4=DC` | large `$26D0` | large |
| 1054 | 1 | `$F7C5=F4` | large `$2700` | large + large |
| 1378 | 2 | `$F7C6=D4`, boundary from prior state | small `$2760` | large + small (large retires 1383) |
| 1456 | 3 | `$F7C7=EC` | `$26D0`, converted to extra Charlie | extra + small |
| 1575 | 4 | `$F7C8=DC` | large `$2700` | extra + large briefly |
| 1716 | 5 | `$F7C9=E4` | large `$26D0` | large + large |
| 1966 | 6 | `$F7CA=DE`, boundary from prior state | small `$2760` | large + small |
| 2119 | 7 | `$F7CB=1B` | large `$26D0` | large + small |
| 2133 | 8 | `$F7CC=EE` | large `$2700` | large + large |
| 2499 | 9 | `$F7CD=F6` | large `$26D0` | large + large |
| 2677 | 10 | `$F7CE=1B`, boundary from prior state | small `$2760` | large + small |
| 2913 | 11 | `$F7CF=DE` | large `$26D0` | large + small |
| 3025 | 12 | `$F7D0=EE` | large `$2700` | large + large |

Fire pots use records `$24B0/$24F0/$2530` and the distinct `$7750+` object
path/table near `$F824`; they do not share the hoop allocator. The hidden coin
uses `$2570`. Neither path was changed in this scoped correction.

## Legal simultaneous combinations proven by MAME

Across frames 943–3040, the captured hoop signatures were:

- one large: 1036 frames;
- two large: 416 frames;
- one small: 445 frames;
- one large + one small: 74 frames;
- extra Charlie only: 70 frames;
- extra Charlie + small: 49 frames;
- extra Charlie + large: 8 frames.

The trace proves:

- one large + one small: **yes**;
- a small followed by a large while the small remains active: **yes**
  (`1966 -> 2119`, and `2677 -> 2913`);
- a small admitted after a large while that large remains active: **yes**;
- large + small + large simultaneously: **not observed**;
- two large + one small simultaneously: **not observed**;
- any other three-ring combination: **not observed**.

Native no longer authors combinations. They arise from the same selector,
reserved slot, three-slot ordinary allocator, course bytes, and fixed-point
timing.

## Exact small-ring collision

`$70DC-$71C8` scans `$26D0` through `$2760`, so the reserved small ring is
collision-tested. `$7130-$7192` performs one point/Manhattan test:

1. `horizontal = abs(object_x_high - $40)`;
2. if `horizontal >= $0E`, it is outside the collision window;
3. rider Y is byte `$2644`;
4. only for `$2760`, `$714B` adds `$10` to rider Y;
5. subtract `$B6`;
6. a negative result on `$2760` enters `$7157`, its safe opening/reward path;
7. otherwise failure occurs when
   `(rider_y+$10-$B6)+horizontal <= $1C`;
8. a value greater than `$1C` exits safely below the lethal boundary.

No object Y, X low byte, previous-frame coordinate, rendered ellipse, explicit
direction, or explicit jump-state flag participates. Collision executes before
the movement, jump, scheduler, and sprite-buffer updates for that board frame.

The six requested coordinate cases are recorded in
`small-ring-collision-cases.csv`. A clean center at rider Y `$95` reaches the
reward branch; upper fire (`$A6`), lower fire (`$C0`), and both horizontal
edges (`X=$33/$4D`, rider Y `$AF`) reach `$7192 -> $7C47`; the physically lower
grounded point shown in case F is beyond the inclusive `$1C` boundary and exits
safely. Native uses this exact branch formula rather than a harmless ring or an
HD-art rectangle.

`$7157-$718A` also proves the small ring has reward logic. `$25E1` selects BCD
1000, 2000, 3000, 4000, then 5000 points and saturates at state four.

## Extra Charlie, separate from obstacle scheduling

Original state byte `$220A` is independent of hoop selection:

- 0: entitlement available;
- 1: reward pending;
- 2: converted object active;
- 3: collected/consumed.

At `$767E-$7684`, only a newly allocated **ordinary** slot can be converted to
the hanging Charlie when `$220A==1`. `$7687` records that object in `<$BF`.
The reserved `$2760` path does not read or alter this decision. `$71A0` handles
collection, increments the life count, and advances the shared state.

Native uses one persistent Stage 1 entitlement for both supported triggers:

- the third opening backward jump;
- jumping backward through a previously cleared large hoop.

Whichever trigger happens first sets the pending/consumed state. The other
trigger cannot produce a second extra Charlie. Death/restart does not reset the
flag; only a new Stage 1 course does. The pending reward converts the next
ordinary admission exactly as `$7684` does and never causes, suppresses, moves,
or delays a small ring or large hoop.

## Full-course comparison

`tools/trace_level1_course_objects.lua` records every frame, course byte read,
state write, ordinary/reserved slot, component code/attribute, fire-pot slot,
coin slot, board-frame byte, and movement command.

`tools/compare_level1_course_scheduler.py` independently replays
`$7539-$7554/$7607-$76F8` from the captured initial state and movement commands.
It compares course index, course state, `<$BB`, activation accumulator, object
type/slot/X, and the active hoop signature for every frame. Type-specific
reward retirement edges are consumed from the MAME snapshots because they run
outside the hoop scheduler; admissions and movement are replayed.

Result: **2,098 consecutive frames synchronized**, MAME 943–3040, with no
scheduler/signature divergence. The result is in
`mame-native-full-course-comparison.csv` and the two per-frame signature CSVs.

The previously accepted successful-large-hoop trace was also rerun: all 120
core movement, jump, collision, landing, score, rider-state, and anchor fields
remain byte/field identical to the accepted baseline.

## Preserved regressions

- Airborne direction remains latched at takeoff.
- Initial start-line LEFT behavior remains intact.
- Fixed large-hoop front/back layering remains intact.
- Jump samples, movement speed, large-hoop collision, landing and scoring are
  unchanged.
- Rider A/B/C selector, production assets, scale and anchor are unchanged.
- Camera/course presentation is unchanged.
- Events 2–4 are unchanged.
