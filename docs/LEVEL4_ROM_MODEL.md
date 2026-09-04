# Event 4 (rolling balls) board model

Clean-room notes on the `circusc4` stage handler at `$97D8-$9F0C`, read from
MAME's decrypted KONAMI-1 opcode view and verified frame by frame against
headless captures (see "Verification").  Addresses are ROM addresses, `$xxxx`
RAM is the board's work RAM.  `src/main.cpp` quotes the routine addresses next
to the code.

## Screen and records

The board is the same 224x256 screen as Event 3, with the 80-row scoreboard on
top.  A record's `$4` (row) and `$6` (column) place a 16x16 cell whose left
edge is `column - 16` and whose top edge is `row`; two-cell composites span
`column - 16 .. column + 15` and `row - 16 .. row + 15`.  The native canvas
maps a row `r` to `2.5 * r - 8` and a column through the usual 480/224 scale.

| Record | Address | Purpose |
| --- | --- | --- |
| Charlie | `$2400` | `$2` state, `$4` row, `$6` column, `$7` phase (1 rising, 2 falling), `$8` landing velocity, `$9` launch velocity, `$A` direction, `$17:$18` 8.8 velocity, `$1A` latched jump, `$1B` latched joystick, `$27:$28` idle timer, `$2D` countdown |
| Ridden ball | `$2440` | `$2` state, `$4` row, `$6` column, `$29` which way it has to travel to get back under Charlie |
| Rolling balls | `$2480/$24C0/$2500/$2540` | `$2` state, `$5` half-column accumulator, `$6` column |
| Objects | `$2600-$26B0` | twelve slots: score popups (state 0) and the cells of the FAR OUT / GREAT / OH NO callouts (state 2) |
| Plaques | `$26C0-$26E0` | the three cells of the distance sign, row `$E0` |

Scroll: `$2203:$2204` starts at zero and moves one column per frame while
Charlie rides to the right, so the page byte counts down (`$FF` = page 1).
Progress in columns is `-$2203:$2204`.  Once the page byte is `$F8` (progress
1793) the scroll stops and Charlie's own column moves from `$50` towards `$E0`
(`$99C4`); moving left on that page first walks him back to `$50`
(`$99AF`) and only then scrolls again.

## Frame order (`$97D8`)

1. `$97F1` the ridden ball against the four rolling balls.
2. `$9D51`: rolling ball spawning (`$9D73`) and motion (`$9E51`).
3. `$9872` Charlie's state handler (`$FAC6`), followed by `$9A18` (plaque
   re-entry and map streaming) because `$9872` pushes it as the return
   address.
4. `$9BF8` the ridden ball's state handler (`$FAD9`).
5. `$7D65` the object slots, `$BB73` the bonus, `$81FA` the screen flash.

Collisions therefore use the positions of the previous frame, and the rolling
balls see the previous frame's direction.

## Charlie (`$98BF`, `$9943`)

- `$8532` latches the joystick into `$241B` (1 left, 2 right) and turns the
  button into a one-shot `$241A` through the `$28F0` held flag.  A press
  during a fall below row `$68` is buffered in `$28D3` and then discarded on
  the landing, which is why the cabinet ignores an early second press.
- Standing still (`$A,X == 0`) runs an idle timer: `$28,X` gains 4 a frame and
  carries into `$27,X`.  On the third carry — 192 frames — Charlie slips off
  the ball (state 2).  Any direction change resets it.
- The jump is the shared arc: rising `row -= velocity_hi; velocity -= $20`
  from `$0420`, then a fall `velocity += $20` until the high byte is back to
  the launch value, 57 frames in all, 88 columns long while moving.  The apex
  frame runs the falling step of `$8425`.
- Landing (`$9A42`): the ball must satisfy
  `|ball + 16 - (column + 24)| < 18` (`+12` moving left).  Standing still the
  ridden ball is tested first, so a stationary jump always comes back down.
  A landing while moving is worth 100 points (`$8ED2`); `$28EF` counts them
  and feeds the difficulty.  The record of the ball landed on is swapped into
  `$2440` (`$9AC8`), and `$29` records which way it must roll to get back
  under Charlie — it closes at two columns a frame (`$9CCB`).
- A landing ends by pulling its own return address off the stack (`$9B0D`),
  so the goal test and the second joystick latch at `$9A11`/`$9A14` only run
  when nothing was underneath.
- Missing (`$9A76`) sets state 2: Charlie slides one column a frame in his
  travelling direction while falling `+$20` a frame until row `$D0`, then
  state 8 with the `$EB23` cells and a 40-frame countdown.

## The close-ball bonus (`$9A84`, `$9B10`, `$9B51`)

When Charlie lands with a second ball still active, `$2888` starts a 16-step
background flash, and if that ball is within 64 columns behind him (moving
right) or between columns `$50` and `$78` (moving left) the board awards a
bonus: `$9B51` takes two object slots for the number, and `$9B10` sets its
value from the distance between the ball and Charlie — 400 at five columns or
more, 600 from two, and 2000 when it lands within a column.  `$28D4` counts
the awards; at 16 and 21 of them the bonus clock starts draining two and three
units a frame (`$BBBE`).

## Rolling balls (`$9D73`, `$9E51`)

A ball enters at column `$F1`, row `$C0`, when `$2204` reaches one of the
columns listed for the current page in the `$F1B5` schedule — six tables of
nine pages, each with up to five entries.  The table is chosen by the dip
difficulty, the Stage 4 visit count and the number of landings so far
(`$9E2D` against the `$FAE9` thresholds), so the stream tightens as the round
goes on.  No ball enters while another is within 36 columns of the right edge
(`$9DEF`).  Standing still, one ball rolls in every 256 frames (`$9E3D`).

Moving, a ball closes at one column per frame from the scroll plus a half
column of its own (`$5,X` accumulates `$80`); on the last page, where the
scroll has stopped, only the half column remains.  Walking backwards puts the
balls in state 1, where they drift away at half a column a frame.  A ball
that reaches column `$F2/$F3` is wiped (state 2).

A ball that touches the ridden ball — `|ridden - rolling| < 28` (`$97F1`) —
knocks both out of their rolling states (`$FAC3`) and, if Charlie is not in
the air, puts him in state 7: 32 frames frozen, then a fall of `+$20` a frame
to row `$D0` and the same 40-frame state 8.

## Goal, failure and the bonus

- Goal (`$9BC0`): landing on page `$F8` with the column in `[$A0,$CA)` lifts
  Charlie six rows, sets state 4 and a 160-frame countdown, and places the
  `$EAFE` FAR OUT / GREAT cells.  The stage then clears unconditionally —
  unlike Event 3 there is nothing to collect.
- Failure: state 8 lasts 40 frames, then `$8517` steps the page byte back by
  one (two below `$FA`) and the board restarts.  `$2204` is cleared, so the
  restart begins at the top of that page.
- Bonus: 6410 on a fresh start (`$FB80`), then 4000, 3500 or 3000 by page
  (`$BD1C`).  It counts down one per frame except during the celebration, and
  `$2263` — armed at `$40` by every stage init — starts ticking on the frame
  the digits reach zero; when the value it reads is 2, Charlie is dropped like
  a ball hit, keeping the velocity he had (`$BC79`).

## Verification

`tools/autoplay_level4_headless.lua` runs `circusc4` headlessly, forces Stage 4
after START and logs every board field above once per frame.  Its `auto` mode
reads the same `$F1B5` schedule the board does, so it can play the stage to
the goal.  `docs/diagnostics/level4-replay/` holds five captures: a complete
run to the goal with 18 landings and 7 close-ball bonuses (`auto-clear`),
three deaths and restarts from riding into the balls (`right-deaths`), a
left/right excursion with a death on a later page (`left-clear`), standing
still until a ball arrives (`idle-fall`), and a bonus time-out with the digits
poked to 0050 (`timeout`).

```sh
./build/circus_charlie_hd --replay capture.csv --replay-event 4 \
    --replay-output native.csv
python3 tools/compare_level4_replay.py capture.csv native.csv
```

All five captures replay with no mismatch in any column of any playing frame.
