# Event 2 (tightrope and monkeys) board model

Clean-room notes on the `circusc4` stage handler at `$7CE8-$8A6B`, read from
MAME's decrypted KONAMI-1 opcode view and verified frame by frame against
headless captures (see "Verification").  Addresses are ROM addresses, `$xxxx`
RAM is the board's work RAM, `<$xx` is the direct page (`$20xx`).  `src/main.cpp`
quotes the routine addresses next to the code.

## Screen and records

The board is the same 224x256 screen as the other events with the 80-row
scoreboard on top.  A record's `$4` (row) and `$6` (column) place a 16x16 cell
whose left edge is `column - 16` and whose top edge is `row`; Charlie and the
monkeys are two-cell composites (`$8347` mirrors the leading cell downward, so
only the leading row and column are kept).  The native canvas maps a row `r` to
`2.5 * r - 8` and a column through the usual 480/224 scale.

| Record | Address | Purpose |
| --- | --- | --- |
| Charlie | `$2400` | `$2` state, `$4` row, `$5` half-column, `$6` column, `$7` phase (0 walking, 1 rising, 2 falling), `$8` landing velocity, `$9` launch velocity, `$A` direction, `$17:$18` 8.8 velocity, `$1A` latched jump, `$1B` latched joystick, `$27:$28` idle timer, `$2D` countdown |
| Monkeys | `$2440-$2500` (brown), `$2540-$25C0` (purple) | `$2` state, `$5` half-column, `$6` column, `$7` phase, `$8` landing velocity, `$17:$18` velocity, `$1D` post-leap timer, `$30` purple flag, `$31:$32` the brown record a purple is leaping over |
| Goal stand | `$2760` | `$2` state, `$4` row, `$6` column: the pole and platform that appear on the last page |
| Objects | `$2600-$26B0` | twelve slots: score popups (state 0) and the FAR OUT / GREAT / OH NO callout cells (state 2) |
| Plaques | `$26C0-$26E0` | the three cells of the distance sign, row `$E0` |
| Bird | `$2700-$2720` | the perfect-clear bird and the bag it carries |
| Coins | `$2730-$27F0` | thirteen coin slots the bird pours out |

Scroll: `$2203:$2204` starts at zero and moves one column per two frames
(`$2205` toggles), so the page byte counts down (`$FF` = page 1).  Progress in
columns is `-$2203:$2204`.  Once the page byte is `$F8` the scroll stops and
Charlie's own column moves from `$40` toward `$C0` (`$83B5`/`$83DF`); moving left
on that page first walks him back to `$40`.

## Frame order (`$7CE8`)

1. `$80C2` Charlie against the monkeys: a fatal touch.
2. `$7D06` monkey against monkey: a purple that catches a brown starts a leap.
3. `$8804` = `$8818` monkey spawner then `$8809` the monkey records.
4. `$7DF2` the perfect-clear bird records.
5. `$8057` the goal stand record.
6. `$7D65` the object slots (popups and callouts).
7. `$8125` Charlie's state handler (`$FA1D`), followed by `$9070` (plaque
   re-entry and map streaming) pushed as the return address.
8. `$BB73` the bonus, `$81FA` the screen flash.

Collisions therefore use the positions of the previous frame.

## Charlie (`$815B`, `$8362`, `$8425`)

- `$8532` latches the joystick into `$241B` (1 left, 2 right) and turns the
  button into a one-shot `$241A` through the `$28F0` held flag, with the same
  early-press buffer (`$28D3`) and its discard on the landing as Event 4.
- Standing still runs the idle timer: `$28` gains 4 a frame and carries into
  `$27`; the third carry (192 frames) drops Charlie off the rope (state 2).
  Any direction change resets it.
- The jump is a fixed arc from `$0420`: rising `row -= velocity_hi;
  velocity -= $20` until the high byte reaches zero, then falling
  `velocity += $20` until it is back to the launch value, 57 frames and 88
  columns while moving.  The takeoff direction is the latched joystick; it can
  also be forced from the `$689B` START-line schedule (the `START`, `60M` ...
  markers), which is how the attract mode jumps by itself.
- Landing (`$84BC`/`$8347`): when the fall velocity equals the launch value
  Charlie is grounded again.  There is nothing to land *on* here -- the rope is
  continuous -- so the only test that ends a life is the collision at `$80C2`.
- Goal (`$8752`): landing on page `$F8` with `$28EE` set (the stand was
  reached) lifts Charlie onto the platform, sets state 4, the `$EAFE` FAR OUT /
  GREAT cells and a 160-frame countdown, then clears the stage.

## Monkeys (`$8818`, `$88E9`, `$897F`, `$7D06`)

- The spawner reads a per-frame schedule pointer `$288D` seeded by `$81A6`
  from the `$F2C0` tables (six difficulty tables of nine page pointers, three
  monkeys wide is not the shape here -- each entry is a `(column, purple)`
  pair and the list runs until `$FF`).  When the running `$2204` matches an
  entry's column a monkey spawns at column `$F1`, row `$88`: a brown one in a
  free `$2440-$2500` slot, a purple one (`purple = 1`) in `$2540-$25C0`, with
  `$30` set to the flag.
- Walking (`$88E9`): a brown monkey closes at 1.25 columns a frame (a half
  column of its own plus one column of scroll), a purple one at 2.5; on the
  last page, where the scroll has stopped, they walk 1 and 1.5 and Charlie's
  own motion adds the rest.  A monkey that reaches column `$F2/$F5` is wiped
  (state 2).
- A purple monkey that comes within 24 columns of a brown one ahead of it
  (`$7D06`) leaps: it takes state 8 and the `$EBE9` arc (the same 57-frame
  jump shape), the brown one freezes in state 9 until the purple lands, and a
  short `$1D` timer (state 1) plays the turned pose before it walks again.
- Charlie dies (`$80C2`) when a monkey's column is within 13 of his `+ $13`
  and its row within 16 of his `+ 8`: state 7 for both, then the fall.

## Failure, the goal stand and the bonus

- A hit sets state 7: Charlie is frozen for a few frames, then falls
  `+$20` a frame to row `$D0`, state 8 with the `$EB23` OH NO cells and a
  40-frame countdown.  `$8509` then steps the page byte back (`$8517`) and the
  board restarts on that page with the bonus reset by `$BB25`.
- The goal stand (`$8057`/`$8097`): on the last page the pole and platform
  enter as record `$2760`; walking into it (`$8015`) sets `$28EE`, and the
  jump that follows lands on the platform for 5000 points (`$80B2`).  Reaching
  the goal without the stand still clears the stage but skips the 5000.
- Perfect clear (`$7EB1`/`$7F3B`, shared with Event 3): if `$220A` is still
  zero at the goal -- which only happens when Charlie never turned to run,
  because `$869B`'s START-line schedule sets `$220A` the moment he does -- the
  bird flies in from the right pouring coins, each worth `$D140[min($220F,5)]`.
- Bonus: 6410 on a fresh start, then 4000/3500/3000 by page, counting down one
  a frame; `$2263` (armed at `$40` by the stage init) starts ticking when the
  digits reach zero and drops Charlie like a hit two frames later.

## Verification

`tools/autoplay_level2_headless.lua` runs `circusc4` headlessly, forces Event 2
after START and logs every board field above once per frame.  Its save-state
`search` mode plays the stage to the goal by rewinding each fatal jump and
retrying its timing, and its `inputs` mode replays the `input` column of a
capture (used to reproduce a search run and snapshot it).
`docs/diagnostics/level2-replay/` holds five captures: a full run to the goal
(`auto-goal`), a run that also takes the 5000-point stand (`stand-clear`), a
perfect clear with the bird and coins (`perfect-clear`), three deaths and
restarts (`right-deaths`), a bonus time-out (`timeout`), a left/right excursion
(`left-clear`) and a stand-still fall (`idle-fall`).

```sh
./build/circus_charlie_hd --replay capture.csv --replay-event 2 \
    --replay-output native.csv
python3 tools/compare_level2_replay.py capture.csv native.csv
```

The native `--replay-event 2` output reproduces every capture's first full life
frame for frame -- Charlie's walk and jump arc, the monkey schedule, their
walk, leap and freeze, the collisions, the scroll, the plaques and the jump
bonuses all match column for column.  A few fields are still being tightened
after the first death: the spawn counter is one frame out across a mid-stage
restart, and the floating score popups and callout cells do not yet sit on the
exact rows and columns the cabinet places them.  `tools/compare_level2_replay.py`
reports which columns still differ.
