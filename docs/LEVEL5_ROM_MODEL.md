# Event 5 (pony and springboards) board model

Clean-room notes on the `circusc4` stage handler at `$9F0D-$A8E8`, read from
MAME's decrypted KONAMI-1 opcode view and verified frame by frame against
headless captures (see "Verification").  `src/level5_types.inc`,
`src/level5_logic.inc` and `src/level5_draw.inc` quote the routine addresses
next to the code.

## Variables

The stage keeps its state in the direct page (`$20B0-$20C2`) and in eight
16-byte springboard records at `$2900`.

| Address | Purpose |
| --- | --- |
| `$B0` | airborne (0 riding, 1 in the air) |
| `$B1` | the pace: `$FD` RIGHT (3 columns a frame), `$FE` neutral (2), `$FF` LEFT (1); re-read only while riding and on a landing |
| `$B2` | the pony's column while it runs on alone after a fall |
| `$B3` | the pony's gait (0-3), stepped every ten columns of scroll |
| `$B4:$B5` | Charlie's 8.8 vertical velocity; `$26B4:$26B5` carries his row with its fraction |
| `$B6` | set when a springboard was landed on this frame (the rebound) |
| `$B7` | Charlie's feet row (`$26B4 + $14`); `$B0` on the pony |
| `$B8` | springboards landed on in one flight |
| `$B9` | falls this stage |
| `$BA` | frames left lying on the grass (`$38`) |
| `$BB` | the time-out counter once the bonus is gone |
| `$BC` | the tumble frame counter after a double bounce |
| `$BD-$C2` | the goal stand: state, columns drawn, entry column, VRAM pointer, landing column |
| `$2203:$2204` | scroll page and column; `$2208` springboards spawned, `$220A` wasted jumps and double bounces, `$220B` the double-springboard chain |

A springboard record: `$0` state (1 waiting, 2 being drawn, 3 live), `$1`
gap to the next spawn, `$2` pole height (1-7), `$3` columns left to draw,
`$4` platform type (`$12` high, `$13` middle, `$14` low), `$5` entry column,
`$6:$7` VRAM pointer, `$8` screen column, `$9` platform row (`(type + 1) *
8`), `$A/$B/$C` the half widths (`$B = (height - 1) * 4 + 9`, `$A = $B - 3`,
`$C = $B + 4`), `$D/$E` the pressed-platform tiles.

## Set-up (`$9FF9`)

Charlie rides at row `$9C` column `$30`, the pony at `$B0/$2C`, the plaque
cells at row `$E0` columns `$48/$38/$28` from the `$EF25` table (three per
page).  The first springboard record waits with a gap of 8.  The bonus is
5360 on a fresh start, then 4800/4000/3000 by half the page after a miss.
The scroll steps back a page (`$A0E3`) so the first tick lands on page 0.

## The playing frame (`$A0EA`)

1. The goal stand (`$A664`), when its state is set.
2. `$BB73`: the bonus, one tick every three frames.
3. `$BA1F`: the background flasher started by a landing.
4. Each springboard by state: waiting (`$A328`), drawing (`$A3CB`), live
   (`$A2EC`), which includes Charlie's collision (`$A46C`).
5. `$A115`: a landing reverses the fall (`$FFE8 - velocity`).
6. Airborne: gravity `+$18` on the 8.8 velocity, the tumble cells every
   eight frames after a double bounce, and the landing back on the pony
   when the feet reach `$B0` (`$A18C`), which counts a wasted jump or a
   double bounce in `$220A`.
7. Riding: the pace from the stick and the jump on the button's edge
   (`$A1AC`, take-off velocity `$FD00`).
8. The scroll by the pace, with the gait every ten columns and the plaque
   re-entry, and the goal page (`$11`) that cancels waiting springboards.

### The schedule (`$A32E`)

A waiting record spawns when its gap is under 3.  Its entry in the
`$F67C/$F6E4/$F74C` tables (height, type, gap to the next) is
`((dips & $60) >> 1) + (visits - 1) * 16 + spawned`, folded into `$60-$67`
past `$68`.  Every fourth spawn starts a double: the second board copies the
first's platform with the maximum height and a gap of `$34`.

### The landing (`$A46C`)

Charlie's column is fixed at `$40`.  A live board's platform is within reach
while its column is inside `$40 +- $A`; then, if his feet are on rows
`$9 - 16 .. $9 - 13`, a falling Charlie lands (`$A4B3`) and a rising one is
tested against the pole (`$A5E5`): he falls when `($B7 - $9) - ($8 - $40)`
is inside `+- $C`, the diagonal band under the leaning pole.  Riding under a
platform (`$A5C9`) uses the same band once the platform row is within
`$12..$35` above his feet.

A landing scores height points (`$EF8C`: 60/50/40/30/20/10/0 by pole
height) plus type points (`$EF92`: 200/100/80/50/20/10 by platform type),
in BCD, doubled once for every pace step of `$B1 - $B8` counting up to zero
-- 320 for the low platform at the fast pace, 800 for the high one -- and
plays `$4B`.

### The fall (`$A5FE` / `$9F53`)

OH NO takes the six cells at rows `$52/$5A`; Charlie drops with the same
8.8 gravity, the pony runs on at `$B1` columns a frame, and once the feet
reach `$B0` he sits (`$31`) for `$38` frames before `$9F24` takes the live
boards off the spawn count, steps the page back (capped at `$11`) and
starts the wipe.  The board runs the wipe for 96 frames before `$9FF9`.

## The goal stand (`$A707`)

It enters at the top of page `$12`, is drawn eight columns at a time, and
lands Charlie (`$A795`) when his feet come back down to row `$A1` with its
column `$C2` inside `$24-$5B`; riding into it (`$5C-$69`) or coming down
short (`$16-$23`) is a miss.  The celebration (`$A67A`) counts `$BC` up to
zero, animates GREAT / FAR OUT from `$F102`, and, when `$220A` is zero,
flies the bird in with an extra Charlie.

## Verification

`tools/autoplay_level5_headless.lua` captures the board frame by frame
(modes `right`, `idle`, `auto`, `script` and `search`, a save-state search
that finds a clear; `CIRCUS_L5_PERFECT=1` neutralises `$A1A3` for the
extra-life bird).  `./build/circus_charlie_hd --replay capture.csv
--replay-event 5 --replay-output native.csv` (with `--replay-visits 2` and
`--replay-perfect` where the capture used them) replays it and
`tools/compare_level5_replay.py` compares every column of every playing
frame.  The eight captures in `docs/diagnostics/level5-replay/` -- deaths at
the fast pace, jumps, a full clear, a perfect clear with the bird, the
time-out, idle riding and a second visit -- replay with no mismatches.
