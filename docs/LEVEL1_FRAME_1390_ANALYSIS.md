# Level 1 frame 1390 first-divergence analysis

## Scope

This pass covers only native trace frame 68 / `circusc4` frame 1390. It does
not change the jump table, obstacle positions, collision geometry, camera
constants, or artwork.

The reproducible trace is `tools/trace_level1_composite.lua`. It records both
raw sprite-RAM banks, all six rider object records, all four active obstacle
object records, the jump and scroll accumulators, and the exact failure-entry
register state for MAME frames 1375 through 1410.

## Hardware format

MAME's official
[`circusc.cpp`](https://github.com/mamedev/mame/blob/master/src/mame/konami/circusc.cpp)
driver treats sprite RAM as two buffered banks. Each
hardware entry is four bytes: code, attributes, X, and Y. Code bit 8 is
attribute bit `0x20`; the low attribute nibble is color; `0x40` and `0x80`
are X/Y flip. VBLANK copies the selected half of raw sprite RAM to the display
buffer, so a state write is presented one frame later.

## Rider and lion composite

The logical rider is always the same six object records, `$25f0`, `$2600`,
`$2610`, `$2620`, `$2630`, and `$2640`. They feed hardware sprite slots 31
through 36. No other slot takes ownership of Charlie or the lion during the
transition.

| MAME frame | Object Y rows | Object codes | Display-bank hardware X | Hardware Y rows |
| --- | --- | --- | --- | --- |
| 1375 | `B2/A2` | `5C 5B 5A / 59 58 57` | 184/168 then 181/165 | 171,187,203 |
| 1387 | `CA/BA` | `5C 5B 5A / 59 58 57` | 202/186 then 199/183 | 171,187,203 |
| 1388 | `CD/BD` | `5C 5B 5A / 59 58 57` | 205/189 then 202/186 | 171,187,203 |
| 1389 | `D0/C0` | `5C 5B 5A / 59 58 57` | 208/192 in the newly written bank | 171,187,203 |
| 1390 | `D0/C0` | `38 37 36 / 35 30 2F` in one bank | 208/192 | 171,187,203 |
| 1391–1410 | `D0/C0` | `38 37 36 / 35 30 2F` in both banks | 208/192 | 171,187,203 |

At frame 1390, hardware slots 23 through 28 additionally become the separate
six-tile failure effect (`18 17 16 / 18 17 16`). They do not replace the rider
composite. This rules out a composite-slot transition as the cause of the
visible Y stop.

## State and object evidence

- Jump accumulator `$20b5:$20b6` advances through `02BC` at frame 1388 to
  `02D8` at frame 1389, then remains `02D8` through frame 1410.
- Animation selector `$20b4` remains `01`; the failure routine directly
  replaces the six sprite codes instead of selecting a normal jump frame.
- Scroll `$2203:$2204` advances to `0198` at frame 1389 and remains `0198`
  through frame 1410. The stop is therefore not a scroll/display illusion.
- The active obstacle is object `$26d0`; `$2700`, `$2730`, and `$2760` remain
  inactive. From frames 1375–1389 its X high byte moves `68,66,...,4C`, Y is
  `9C`, sprite code is `E4`, and animation attributes cycle `03/05/04`.
- At logical frame 1389 the collision routine reads `$2644 = C0` at PC
  `$7140` with `U=$26d0`, object X `4C`, object Y `9C`, code `E4`, attribute
  `05`.
- That branch enters `$7c47`. The first failure-pose write is `$264e=2f` at
  PC `$7c4d`, still with `U=$26d0`. It then writes the remaining five rider
  codes, builds the effect composite, writes `$20cb=40`, and advances `$2800`
  to state `03`.

## Cause

The visible rider stopping at source Y 208 is a **collision/status state
transition**. It is not a composite-slot handoff, normal animation offset, or
scroll behavior. The apparent one-frame discrepancy comes from MAME's
buffered sprite presentation: collision logic changes the staged six-tile
composite during logical frame 1389; the updated bank is visible at frame
1390 while retaining the preceding rider pose.

## Native mechanism

Native Level 1 now preserves the preceding vertical composite sample when an
existing hoop collision enters `Scene::Crashed`, while retaining that tick's
horizontal movement/camera update. It also rolls the jump-state index back to
the buffered sample. This reproduces the board mechanism without changing
the jump samples, hoop placement, collision dimensions, camera, or art.

The native trace now includes scene/alive/crash state and nearest-hoop world,
screen, previous-screen, opening, cleared, and overlap fields.

## New first divergence

The mechanism itself is now present, but the deterministic native run does
not enter it at native frame 68. At that frame native remains `Playing`, its
nearest hoop is at logical screen X `529.865356`, and `hoop_overlap=0`, while
MAME has already entered `$7c47` for active object `$26d0` and presents the
failure composite at frame 1390.

Adding hoop state to the comparison reveals an earlier disagreement that the
old rider-only trace could not see. At the beginning of the requested window,
native frame 53 / MAME frame 1375, MAME's active `$26d0` hoop is 40 source
pixels ahead of the fixed rider axis (`X=68` versus `40`), or `85.714286`
native logical units. Native's nearest authored hoop is `496.167358` logical
units ahead. Thus the new first divergence is **native frame 53 / MAME frame
1375: active-hoop object position/state**. Changing it requires the separately
scoped hoop-object activation/position pass, which this task explicitly
excludes.
