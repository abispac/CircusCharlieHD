# Level 3 (trampolines) board model

Clean-room notes on the `circusc4` stage handler at `$8A6C-$97D7`, read from
MAME's decrypted disassembly and verified frame by frame against headless
captures (see "Verification").  Addresses are ROM addresses, `$xxxx` RAM is
the board's work RAM, `<$xx` is the direct page (`$20xx`).  Everything the
native game does in Event 3 follows this model; `src/main.cpp` quotes the
routine addresses next to the code.

## Screen and records

The board is a 224x256 screen with the 80-row scoreboard on top.  Record
bytes `$4` (row) and `$6` (column) place a 16x16 cell so that its left edge
is `column - 16` and its top edge is `row`; two-cell composites (Charlie,
performers) span `column - 16 .. column + 15` and `row - 16 .. row + 15`.
The native canvas maps a row `r` to `2.5 * r - 8` and a column through the
usual 480/224 scale.

| Record | Address | Purpose |
| --- | --- | --- |
| Charlie | `$2400` | `$2` state, `$4` row, `$6` column, `$7` phase (1 rising, 2 falling), `$8:$3A` landing velocity, `$A` direction, `$17:$18` 8.8 velocity, `$1B` latched joystick, `$2D` countdown, `$37` stationary rebound count |
| Performers | `$2440/$2480/$24C0/$2500` | `$6` column, `$37/$39` attack period and timer, `$38` knives left, `$3C` type (0 fire breather, 1/2 juggler) |
| Knives | `$2540-$2570` | `$1` owner, `$2` state, `$4/$6` cell, `$7:$8` velocity, `$9` apex flag, `$A` sway accumulator, `$3` hold |
| Flames | `$2580/$25A0/$25C0` | two cells; `$2` state, `$4/$6` lower cell, `$7:$8` velocity, `$9` apex, `$3` hold |
| Bags | `$2690/$26A0/$26B0` | `$2` state (3 hanging, 4 popup, 6 coin pile), `$6` column, `$9` spawn key, `$A` timer |
| Plaques | `$26C0-$26E0` | the three cells of the distance sign, row `$F0` |
| Presentation | `$2700/$2710/$2720` | bird cells and its bag |
| Coins | `$2730-$27FF` | thirteen coin slots; `$27B0` receives a copy of the bag record |

Scroll: `$2203:$2204` starts at zero and moves two columns per frame, so the
page byte counts down (`$FF` = page 1).  Progress in columns is
`-$2203:$2204`.  Once the page byte is `$F8` (progress 1794) the scroll stops
and Charlie's own column moves from `$50` to `$C0` (`$8DDD`); moving left on
that page first walks him back to `$50` and only then scrolls again.

## Frame order (`$8A6C`)

1. `$8A93` pressed-drum tile timer (`$28F4`, 8 frames).
2. `$8B3E` flame collisions, `$8BA9` knife collisions (`$8BF9` performer body
   collisions only in the `$2207` knife-thrower variant of the second loop).
3. `$923E`: performer spawn (`$93F8`), performer records (`$95A9`), flames
   (`$9273`), knives (`$9343`).
4. `$7D65`: bag and goal-object records.
5. `$8C50`: Charlie's state handler (`$FA53`: 1 `$8C85`, 4 `$8FD7`, 7 `$9015`,
   8 `$8509`), followed by `$9070` (plaque re-entry, map streaming).
6. `$BB73` bonus countdown, `$7DF2` presentation records, `$7F3B` coin
   spawner, `$8AA8` bag spawn.

Collisions therefore use the positions of the previous frame; performers
and bags see the scroll of the previous frame, the bag spawn the new one.

## Charlie (`$8C85`, `$8D9D`, `$8DE7`)

- `$8532` latches the joystick bits into `$241B` every frame (1 left,
  2 right).  The direction (`$240A`) only changes at a landing (`$8F54`).
- Rising: `row -= velocity_hi; velocity -= $30`.  The apex is the frame in
  which the velocity reaches zero, or (stationary rebounds, `$37 != 0`) the
  first frame whose high byte is already zero.  A fourth stationary apex
  (`$37 == 4`) is the roof: `row += 2`, state 8, sprite `$ECBA`, the
  `$EB23` "OH NO" objects and a 40-frame countdown.
- Falling: `row += velocity_hi; velocity += $30`; landing when the velocity
  equals the launch value, then `row += launch_hi`, `$8ED2` (20 points when
  moving right), `$8F8D` (pressed tiles), `$8F54` (phase 1, direction :=
  joystick, sound), `$8EFC` (launch velocity), `$9160` (goal).
- Launch velocity: moving `$0420` (44 frames, 88 columns) or `$03C0`
  (40 frames, 80 columns) when `$2204` is in `[$48,$58)` moving right or
  `[$F8,$08)` moving left — the gap between the third drum of a page and
  the first of the next.  Stationary: `$FA65` = `$0450, $0510, $0630, $0810`
  (42, 50, 62 frames; apex rows `$8B, $7A, $5A`) with `$37` incremented.
- Bag pickup (`$8E5A`): while rising on the third stationary rebound with
  the row in `[$50,$68)`, a hanging bag with `|bag - column| < 16` and
  `|bag + 8 - (column + 16)| < 19` becomes a 32-frame popup worth
  `(min($28F1 + 2, 8) + 1) * 100` (300, 400, ... 900); `$28F1` and `$28F2`
  increase and persist for the whole game.
- Goal (`$9160`): landing on page `$F8` with the column in `[$A0,$CA)`:
  `row -= 10, column += 4`, state 4, sprite `$EAAA`, the `$EAFE` "FAR OUT"
  and "GREAT" cells (blinking 16 frames on/off), a 160-frame countdown and
  `$7F14` (see "Perfect clear").  After the countdown the stage clears once
  `$220A` is non-zero.

Drums are tile rows 6-11, 17-22 and 27-0 of the repeating 256-column page:
centres at columns 80, 168 and 256 (+256 per page).  The last reachable
drum (1960) is the goal; the plaques (`$26C0` props re-entering at
`$2204 == $80/$70/$60`) read START at 120, then 60M 376, 50M 632, 40M 888,
30M 1144, 20M 1400, 10M 1656 and GOAL 1912.

## Performers (`$93F8`, `$95B7`)

Only the first free performer record is a candidate each frame; it is used
when `$2204` (before the scroll) equals `$C4` (slot 0), `$74` (slot 1) or
`$18` (slot 2) while moving.  The type comes from `$F517`: six difficulty
tables (dip difficulty / 2, plus loop adjustments) of nine page pointers
(page = `-$2203`, minus one when moving left) with three slot bytes each:
0 none, 1 fire breather, 2 juggler with one knife, 3 juggler with two.  The
default dip setting selects table 1:

```
page:   0/1     2       3       4       5       6       7/8
slots:  0 1 0   2 0 0   2 0 1   0 1 1   0 1 1   0 1 1   1 3 0
```

A new performer appears at column `$F1`, row `$E0`, and follows the scroll
until it wraps back to `$F1`.  Its attack period is `$FAAB[difficulty / 2 +
loop + thresholds passed by $28F2 in $FAA1]`: 57 frames at the default
setting.  A fire breather spawns a flame every period (if one of the three
records is free): lower cell at row `$D2`, column `+4`, breathing pose
`$ECC3` for 32 frames.  A juggler throws its one or two knives once each
(period apart) and then keeps them in the air forever.

## Projectiles

- Flame (`$9273`): eight frames at the mouth, then a rise from velocity
  `$0400` minus `$10` per frame (100 rows over 49 moving frames), an apex
  frame and eight hover frames, 65 frames in all.  Hit test (`$8B63`):
  `|column + 8 - flame| < 8` and `|flame_row - 4 - row| < 10`.
- Knife (`$9343`): the same rise, then a fall from zero velocity plus `$10`
  per frame; at `$0300` the juggler shows `$ECFC` (catch), at `$0400` the
  knife is caught, hidden for 16 frames and thrown again.  While flying it
  drifts one column to the right every eight frames (`$93CA`).  Hit test
  (`$8BA9`): `|column + 8 - knife| < 9` and `|row - knife_row - 8| < 8`.
- A hit (`$8B93`) freezes the projectile (state 4) and puts Charlie in
  state 7: seven frames still, then a fall with `+$20` velocity per frame
  until row `$D8`, then state 8 (`$EA9E`, 40 frames).

## Bags (`$8AA8`, `$7DAE`)

Moving right, after the scroll update, the first `$FA43` entry whose page
byte equals `$2203` must also match `$2204` exactly: `(FF,40) (FE,98)
(FD,98) (FC,40) (FB,98) (FA,40) (F9,98) (F9,08)`.  The second `$F9` pair is
never reached, so seven bags exist, at column `$F0` (240) row `$50`:
progress 192, 360, 616, 960, 1128, 1472, 1640, i.e. eight columns right of
the drums 424, 592, 848, 1192, 1360, 1704 and 1872.  A bag scrolls with the
world; wrapping to column `$F2/$F4` while moving right counts it as missed
(`$220A`), moving left just removes it (it re-spawns later).

## Failure and restart

State 8 lasts 40 frames, then `$8517` steps the page byte back by one (two
from `$F9/$F8`) and the board enters the 96-frame restart phase (the
playfield wipes and is redrawn).  Lives, `$220A`, `$28F1/$28F2` and the
page survive; `$BB25` restarts the bonus at 4000 (pages 0-2), 3500 (pages
3-4) or 3000.  The bonus starts at 5830 on a fresh start, counts down one
per frame except during the celebration, and 62 frames after reaching zero
(`$2263`) Charlie is dropped like a projectile hit.

## Perfect clear (`$7F14`, `$7E67`, `$7F3B`, `$7E7B`)

With `$220A == 0` and no bag still hanging, the bird records enter from the
left edge one column per frame (row `$40`, bag at `$50`) until the bag
reaches column `$B4` (196 frames).  Every eighth frame (`<$14 & 7 == 0`) a
free coin slot launches from the bag with the `$F9B0` lane (column offset
and 8.8 drift, negative when bit 7 is set): the first launch opens the bag
and starts the coin piles (bag records 1 and 2, state 6, `$F9F5`), the
following ones score `$D140[min($220F, 5)]` = 100 points each.  A coin's
vertical velocity gains one row per frame every 32 frames; it disappears at
row `$B0`.  After forty scoring coins `$220A` is raised, which ends the
celebration.

## Verification

`tools/autoplay_level3_headless.lua` runs `circusc4` headlessly, forces
Level 3 after START and logs every board field of the records above once
per frame.  `docs/diagnostics/level3-replay/` holds seven captures: holding
RIGHT with projectiles wiped (`right-clear`), with the `$8B93` writes
neutralised so every flame and knife flies (`right-nodeath`), with three
deaths and restarts (`right-deaths`), the reactive all-bags run with the
bird and forty coins (`bags-clear`), two roof deaths (`roof-clear`), a
left/right excursion (`left-clear`) and a bonus time-out with the digits
poked to 0090 (`timeout-clear`).  `circus_charlie_hd --replay capture.csv
--replay-event 3 [--replay-clear-projectiles | --replay-invulnerable]
--replay-output native.csv` replays the joystick column and
`tools/compare_level3_replay.py capture.csv native.csv` compares every
column of every playing frame: all seven captures replay with no mismatch.
