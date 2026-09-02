# Event 1 behavior reference

This file records the measured `circusc4` board behavior that the native
Event 1 reproduces. `docs/LEVEL1_ROM_MODEL.md` lists the routines and the
frame-exact verification; earlier hands-on estimates that the captures
contradicted have been replaced.

## Measured layout and sequence

- The course is 1775 source pixels long from the start line to the goal
  landing (`$2203:$2204 = $0711` in the manual capture), about 3800 native
  logical units. Distance signs sit at `page * 256 - 24`: 60M at 232, 50M at
  488, 40M at 744, 30M at 1000, 20M at 1256, 10M at 1512; the GOAL plaque is
  page seven's sign at 1768.
- Hoops come from the course stream at `$F7C4` (13 bytes were read in the
  attract capture, 17 in the manual full run, and the stream continues with
  an eight-byte loop for slow players). The small/prize ring is admitted at
  selector boundaries whose phase depends on the frame byte sampled at the
  level start, so its slots differ from attempt to attempt (indices 2/6/10
  in the attract capture, 4/8 in session-01, 2/6 in session-02).
- All large fire rings use the same opening and collision geometry.
- Fire pots are not authored positions. Wave 1 seeds a chain at the first
  page boundary of pages two to four while no pot record is busy: the first
  pot appears 64 source pixels later, then the `$F81C` spacing table places
  the next ones (230, 252, 226 pixels apart) until the chain would pass
  progress 1528. Pages six and seven add two fixed pots each at page offsets
  `$5C` and `$04`, i.e. 1443, 1531 and 1699 (the fourth lies beyond the goal).
- The goal is a striped pedestal with a `GOAL` plaque. Charlie stops on it
  while the crowd flashes `GREAT` and `FAROUT`. The landing itself scores
  nothing.

## Rewards

- Every small ring carries a money bag (`$25E0 = $FF` at admission). Passing
  through the opening (rider row above `$B6` after the `+$10` adjustment)
  awards 1000, 2000, 3000, 4000 and then 5000 for every further ring; the
  ring keeps moving and is also counted as a cleared hoop (+100) on landing.
- A ring that retires with its bag still attached, or a failure, cancels the
  perfect-clear bird.
- Clearing a large ring awards 100 on landing per ring passed during the
  jump. A fire pot passed in the same jump adds 200; a pot passed alone
  awards 500.
- Exactly one chain pot hides the coin: the `(frame byte & 3)`-th pot
  (three counts as zero). The coin is armed by taking off with that pot at or
  behind the rider's column and launches on the following backward landing
  (+800). Catching it in flight awards 3000; it falls back into the pot and
  never launches again.
- The hanging extra Charlie is offered after any backward landing that
  carried a record from behind the rider to ahead of him; the next ordinary
  admission becomes the doll. Catching it (rider row above `$A8`) adds one
  life. A doll carried off the right edge, or active at the time of a
  failure, is offered again; one that scrolls off to the left is lost.

## Perfect Event 1 reward

With no missed bag and no failure, the bird enters from the right during
the crowd cheer, stops with its bag at column `$31`, and eleven coin records
shower down repeatedly at 40 points per launch until the presentation ends
(46 coins, 1840 points, in the captured run).

## Finish and time tally

- The remaining bonus freezes when Charlie reaches the goal and counts down
  one unit per board frame while playing; the hurry music starts at 0499.
- The `FINE!!` screen converts the remaining bonus with the `$FCC9` table:
  `4500+ = 10000`, `4000-4499 = 5000`, `3500-3999 = 4000`,
  `3000-3499 = 3000`, `2500-2999 = 2000`, `2000-2499 = 1000`,
  `1500-1999 = 800`, `1000-1499 = 600`, `500-999 = 400`, `0-499 = 200`.

## Failure

- A collision shows the burning composite for 64 frames; control returns 160
  frames after the collision.
- The course restarts at the beginning of the previous page (two pages back
  from page seven), with every object record cleared, the course index kept,
  and a fresh random small-ring phase and coin pot.
- Local tracks 07 and 09 start together at the collision frame; the Event 1
  music restarts when Charlie respawns.
