# Level 1 board model

This document describes how native Event 1 now reproduces the `circusc4`
board frame by frame, what evidence each rule rests on, and how to re-run the
verification. Addresses refer to MAME's decrypted KONAMI-1 program view
(`tools/dump_reference.lua`). ROM data tables are read from the raw program
file because only opcodes and inline operands are encrypted.

## Evidence

| Source | What it proves |
| --- | --- |
| `manual-level1-capture/session-01` | A complete manually played course to the goal (MAME frames 505-4679): every object record, the bag, both prizes, three landings over pots, the goal, the bird and the coin shower. |
| `manual-level1-capture/session-02` | Two opening backward jumps, the hanging extra Charlie, its collection, and the hidden coin reveal (+800). |
| `docs/diagnostics/level1-replay/headless-three-failures-*.csv` | `tools/autoplay_level1_headless.lua` in headless MAME replaying session-01's joystick until frame 1200 and then holding RIGHT: three deaths, two restarts and the game over. |
| Program ROM `$F7C4-$F81B`, `$F81C-$F84F`, `$EE94-$EEB7`, `$EF07-$EF1E`, `$FCC9-$FCDB` | Course stream, fire-pot spacing, rider pose cells, distance signs, tally rows. |

`--replay` feeds the `player_input` column of any of those captures into the
native game and writes one native row per board frame;
`tools/compare_level1_replay.py` compares it with the emulated state. All
three captures replay with **zero mismatches** in course progress, jump
state, rider row, course index and state, the four hoop records, the three
fire-pot records, the bag state, score, lives and `$220A`.

```sh
./build/big_top_run --replay session-01/capture-state.csv \
    --replay-output /tmp/native.csv --replay-frame-byte 0xbc
python3 tools/compact_level1_objects.py session-01/capture-objects.csv /tmp/objects.csv
python3 tools/compare_level1_replay.py session-01/capture-state.csv /tmp/native.csv /tmp/objects.csv
```

`--replay-frame-byte` supplies `<$14` at the level initialisation tick (the
board's free-running frame byte is not part of the captures; it is
`$BC` for session-01, `$43` for session-02 and `$04` for the headless run).

## Timing

| Board | Native |
| --- | --- |
| Frame `f` runs one game tick using the joystick latched during `f`. | `updateGame` call `u` consumes the input of frame `start + u`. |
| The frame that sets `$2800` to one runs the initialisation only. | `--replay` starts on the following frame. |
| A jump press sets `<$B0` in its own tick; the first displacement sample (row `$CC`) appears in the next tick. | `beginLevel1Jump` runs in the update of the press; the jump table advances from the next update. |

## Course progress

`$2203` is the page, `$2204:$2205` a descending 8.8 page offset. Native keeps
`level1ProgressFixed = page * 65536 - offset` in 1/256 source pixel and derives
page and offset from it (`level1Page`, `level1PageOffset`). RIGHT adds `$180`
per tick, LEFT subtracts `$130` unless the page byte is zero, neutral adds
nothing. World X is `78 + progress * 480/224`.

## Hoop scheduler (`$7607-$76FD`)

- `delta = command - $80` where a LEFT command counts as zero while the page
  byte is zero.
- `delta >= 0`: `<$C2:$C3 += delta` (16-bit wrap).
- `page >= 7 and <$BE < 5`: accumulate only; no admission near the goal.
- Otherwise `<$C2:$C3 += delta`; an admission happens only on borrow.
- Selector `B = $10 + $2208`; `B >= $68` wraps to `($60 | (B & 7))`; the
  reload byte is `[$F7B4 + B]`, i.e. `kLevel1HoopActivationReload[B - $10]`.
- `(B + <$BB) & 3 == 0` is a selector boundary: with `<$BC >= $60` the
  reserved `$2760` small ring is admitted with reload `(<$14 | $80) << 8`,
  `$25E0 = $FF` and `$25EE = $FC00`; otherwise `<$BB` is incremented.
- Ordinary admissions search `$26D0/$2700/$2730` only; the stream advances
  even when the pool is full. `$220A == 1` converts the new record into the
  hanging Charlie.
- `<$BB` and `<$C1` are seeded from `<$14` at every board initialisation
  (`$6EAC-$6EB7`), so the phase of the small rings and the coin pot are random
  per attempt; `$2208` starts at zero and `<$C2` at `$10xx`.

## Hoop movement (`$7539-$7606`)

Each active record adds `delta` to its 8.8 X. A borrow while moving right or
idle retires the record (`$7586`): `<$C4` keeps only its low byte, a reserved
ring sets `<$BD`, and a bag still attached (`$25EE == $FC`) increments the
missed-reward counter `$220C`. A carry while backtracking un-admits the
record (`$759E`): `<$C2` high byte cleared, `$2208` decremented, and a
converted extra Charlie returns to pending.

`$76FE-$774F` brings the last retired object back from the left edge: `<$C4`
grows by `$200`/tick moving right and `$80` idle, shrinks by `$B0` moving
left; on borrow, if no record sits below column `$41`, the reserved ring (if
`<$BD`) or the first free ordinary record is re-activated at X `$0100`.

## Collision (`$70DC-$71E2`)

- Hoops: `abs(X_high - $40) < $0E`; the tracked extra Charlie collects when
  the rider row is below `$A8`; the reserved ring adds `$10` to the rider
  row and rewards the bag when the row is above `$B6`; otherwise failure when
  `(row - $B6) + abs(X_high - $40) <= $1C`.
- Fire pots: any record with `X_high - $29 < $2E` kills a rider whose row is
  `>= $C6` (less than 13 source pixels of lift).

## Jump, landing and scoring (`$71E4-$7335`)

- `$7202`: airborne, progress above 1752 source pixels (`$2203 == 7` and
  `$2204 < $28`, or `$2203 > 7`) and the new row `>= $C5` reaches the goal.
- `$7235`: during the last four descent pixels the joystick is re-sampled and
  a fresh jump press is buffered to start on the landing tick.
- Takeoff (`$7394`) marks records with X in `[$40,$BF]`; a forward landing
  awards 100 per marked record now outside that range (the small ring
  included, the extra Charlie excluded), plus 200 when the pot marked at
  takeoff (`$73AA`, first pot in `[$40,$9F]`) is now below `$40`; a pot
  alone awards 500. Landings past page seven from a pot alone increment
  `<$BE`.
- A backward landing arms the extra Charlie (`$220A = 1`) when an unmarked
  record is now in `[$40,$BF]`, and launches the hidden coin (+800) when the
  takeoff had armed it.

## Fire pots (`$7750-$7902`)

Records `$24B0/$24F0/$2530` hold status (0 free, 1 pending, 2 visible), an
8.8 countdown, an 8.8 X and a flame timer.

- While every record is idle, moving right at the start of pages two to four
  (`$2204 < 2`) schedules `$24B0` with countdown `$4000`.
- Pending: RIGHT subtracts `$180`; on borrow the pot becomes visible at
  X `$FE80`-relative zero, the counter-selected pot is remembered as the coin
  pot, and the next record is scheduled with the `$F81C` spacing byte at index
  `8 + $2209` (wrapping at `$34`) unless the pot would appear beyond progress
  `$5F8`. LEFT adds `$130`; overflow cancels the pot.
- Visible: X moves by the command. Reaching the left edge clears the record
  (and the coin pointer); being carried past the right edge makes it pending
  again and withdraws the following record.
- From page six the `<$C6`/`<$C8` pointers place two fixed pots per page at
  `$2204` windows `$60-$63/$5E-$5F/$5C-$5D` and `$08-$09/$06-$07/$04-$05`.

## Hidden coin (`$7965-$79D9`, `$70EB`, `$72AB`, `$73CC`)

`<$C1 = <$14 & 3` (three becomes zero) chooses the chain pot. The coin
follows its pot at X - 8; a takeoff with the coin at or left of column `$40`
arms it, the next backward landing launches it (+800) with velocity `$FBA0`
and gravity `$1C`; it is caught (+3000) when the coin X is in `[$26,$45]`
and `coin_row + $0C - (rider_row + 8) < $14`; it hides again at row `$D6`.

## Goal presentation (`$79DA-$7C41`)

`$25E0` is cleared, `$2500 = 1`, `<$CA = 0`. With `$220C == 0`: `<$CA = $80`,
`$2500 = 2`, the bird (row `$5F`) enters from the right one column per frame,
and once its bag is at column `$31` the eleven `$2520-$25C0` coin records
launch on timers `$2C, $28, ... $04`, each launch and re-launch (row above
`$D8`) adding 40 points. The presentation ends when `<$CA` has wrapped
`$2500` times.

## Failure (`$7C47`, `$7CC1-$7CDD`)

64 burning frames, then the page steps back by one (two from page seven) and
the restart phase zeroes the offset, clears every record and re-runs the
initialisation; control returns 160 frames after the collision. `$220C`
increments and a converted extra Charlie (`$220A == 2`) becomes pending.

## Presentation data

- Distance signs: `$EF07 + 3 * page`, positioned at `page * 256 - 24`.
- Rider poses: `$EE94` forward A/B/C, `$EEA6` backward D/E/F (D reuses A's
  cells), advanced every 11 source pixels forward or 7 backward, pose A when
  stopped, C while airborne.
- Hurry music: `$BB73` fires when the bonus digits reach `0499`.
- Tally rows (`$C34D`, `$FCC9`): 4500+ → 10000, 4000 → 5000, 3500 → 4000,
  3000 → 3000, 2500 → 2000, 2000 → 1000, 1500 → 800, 1000 → 600, 500 → 400,
  else 200.
