# Event 6 (the trapeze) board model

Clean-room notes on the `circusc4` stage handler at `$A8E9-$B9F2`, read from
MAME's decrypted KONAMI-1 opcode view and verified frame by frame against
headless captures (see "Verification").  The handler works almost entirely
on RAM -- five trapeze records, the sprite table and the direct page -- so
the port (`src/level6_logic.inc`) keeps a byte mirror of `$2000-$3FFF` and
transcribes each routine against it; every capture column is then a board
address.

## Records and variables

Five 48-byte trapeze records at `$2900`: `$0` index, `$1` half-swings so
far, `$2:$3` the swing phase (8.8, `$00-$B4`; `$5A` is the far end of a
swing, `$00/$B4` the vertical), `$4` the swings hung on it (starts at
`(60 - length) / 4`), `$8` amplitude, `$9` rope length, `$A:$B` the phase
step from `$F07E`, `$C/$D` the sine pair of the bar's reach, `$10-$1F` the
rows and `$20-$2F` the columns of its sixteen rope cells (the bar last).

| Address | Purpose |
| --- | --- |
| `$B0` | airborne; counts trampoline bounces without a stick |
| `$B5:$B6`, `$B7:$B8` | 8.8 horizontal and vertical velocity in flight |
| `$B9-$C3`, `$C5-$CF` | the last five bar positions and their quarter difference: the velocity Charlie leaves with |
| `$D1:$D2:$D3` | his column relative to the bar he is on (sign, column, fraction) |
| `$D4-$D6` | the lengths of the current, next and previous ropes |
| `$D7`, `$D8` | the stick, and the pump accumulator (`$18` a frame) |
| `$D9/$DA/$DB/$DC` | the first index of the stage, the current bar, the last bar scored, the last bar of the stage |
| `$DD` | frames the button is ignored after a catch (7) |
| `$DE` | set on a release until the next bounce or bar change |
| `$DF:$E0:$E1` | the goal platform's position (`$C0xx` until page 6 places it), `$E2` its VRAM |
| `$E4-$F2` | three trampolines: position and VRAM, five bytes each |
| `$F3`, `$F4` | the trampoline flash, the points popup timer |
| `$F5` | the time-out: 1 freezes the handler, 2 ends the life |
| `$F6`, `$F7`, `$F8` | the celebration's act and counter, the frames until the restart |

## Set-up (`$A8FB`)

The first index is `((dips & $60) >> 1) + (visits - 1) * 16 + spawned`;
records for indices `-2..+2` come from `$F850` (amplitude) and `$F8B8`
(length).  Charlie hangs on the middle one.  Three trampolines are placed
under the next three bars (one in three is off, by the parity of the index);
the plaque, the rope beads (`$55`, the bar `$56`) and the truss are set, the
bonus is 6000 (4800/4000/3000 by the page after a miss) and `$16` plays.

## The playing frame (`$AAA9`)

1. `$BB73` the bonus, one tick every four frames; `$B924` the time-out.
2. `$BA1F` the flasher, `$B52C` the goal platform's colour.
3. Airborne: the goal landing (`$B556`) when the platform is inside
   `$3C..$64` and Charlie's row is `$68`; falling onto a trampoline
   (`$AAED`: its column inside `$40 +- $20`, his row at `$CC` or below)
   rebounds him with `$F0F4`'s velocities -- straight up, higher with each
   bounce, or sideways at `+- $70` with the stick -- and plays `$45`; else
   the catch (`$AB30`): the current bar within 6 columns of `$34` (9 for a
   decorated bar) and 12 rows of him, which scores `$EF98` by the swings
   spent on the bar he left, resets the new bar to its far end with its
   amplitude scaled by the sine of the catch, and shows the points for `$30`
   frames; else gravity (`+$15`) and the floor (`$B857`) past row `$D8`.
4. Hanging (`$AC10`): a button edge after `$DD` runs out lets go (`$B4E2`)
   with the bar's averaged velocity.
5. `$AC27`: every record in range swings (`$B39C`); airborne the scroll and
   `$D1:$D2` move by `$B5` and the bar changes when he is `length + 8`
   past it (`$AC75/$ACF9`, spawning a new record two ahead and, on even
   indices, a trampoline); hanging the world follows the bar (`$AD6E`) and
   the stick pumps the amplitude (`$ADA9`: +1 every `$100/$18` frames with
   the swing, -1 against it, up to `$5A`).  Page 6 places the goal
   (`$AE2B`).
6. The three rope slots (`$AE97`), the goal and trampoline tiles, the
   popup and the plaque.

Sixteen half-swings on one bar let go of Charlie (`$B3D4`).  A fall shows
OH NO, plays the miss, and waits `$80` frames (`$B8FB`) before the spawn
count and page are adjusted and the wipe starts; the restart comes 96
frames later.

## The celebration (`$B6FE`)

The bag flies from the marker to Charlie (`$B608`), he waves for 256 frames
(`$B635`), then twelve copies of him bounce in a fan (`$B67D`, `$B7E5`)
through two acts before the stage ends.

## Verification

`tools/autoplay_level6_headless.lua` captures the board (modes `idle`,
`jump`, `hold`, `pump`, `script` and `search`).
`./build/circus_charlie_hd --replay capture.csv --replay-event 6
--replay-output native.csv` replays it and `tools/compare_level6_replay.py`
compares every column of every playing frame.  The captures in
`docs/diagnostics/level6-replay/` replay with no mismatches.
