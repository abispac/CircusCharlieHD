# Level 1 original coordinate and horizontal movement model

This note records only facts verified from the `circusc4` ROM and MAME. It
does not claim that jump, camera, obstacle, or collision behavior is fully
synchronized yet.

## Board timing and coordinates

- Board refresh: `6144000 / (384 * 264) = 60.606060... Hz`.
- The rider remains at screen X `$45` in object slots `$25F0` and `$2600`
  during a normal forward run.
- World advancement is represented by the scroll accumulator at
  `$2203:$2204`; it is not represented by increasing the rider screen X.
- The per-frame signed 8.8 movement command is held at direct-page
  `<$B1:$B2`, physical RAM `$20B1:$20B2`.

The native HD canvas is 480 logical pixels wide while the original visible
board is 224 pixels wide, so the current horizontal conversion is
`480 / 224 = 2.142857...` native units per source pixel.

## ROM routines and data flow

- `$8532-$8546`: reads the active-low P1 direction bits and records the
  masked direction at `$241B`.
- `$7344-$7363`: clears/loads `<$B1:$B2`, indexes the direction table, and
  stores the selected signed movement command. The effective store occurs at
  `$7363` in the decoded MAME program view.
- `$7425-$744B`: applies `<$B1:$B2` to scroll accumulator `$2203:$2204`,
  including carry/borrow into `$2203`.

Observed movement commands:

| Input | ROM command | Source displacement/frame | Native displacement/frame |
| --- | ---: | ---: | ---: |
| RIGHT | `FE80` | `-1.5 px` scroll | `+3.214286` |
| LEFT | `0130` | `+1.1875 px` scroll | `-2.544643` |
| released | `0000` | `0 px` | `0` |

The sign reverses in the native world model because the original moves the
background/scroll accumulator while the native game advances Charlie's world
coordinate.

## Verified state transitions

- Direction press selects the full table delta on the next board sample.
- Release immediately writes zero; there is no coast or deceleration state.
- RIGHT-to-LEFT immediately replaces `FE80` with `0130`; there is no braking
  phase.
- A full-speed forward jump continues to use `FE80`; the horizontal command
  does not change at takeoff.

## Reproduction

`tools/trace_level1_movement.lua` produces the four MAME CSV traces selected
by `CIRCUS_LEVEL1_TRACE`: `hold-right`, `right-release`, `right-left`, and
`forward-jump`.

The native executable accepts the equivalent `--trace FILE.csv --trace-mode
MODE` options and exits after 120 deterministic board frames.

`tools/compare_level1_movement.py` aligns the first effective movement sample,
converts the ROM 8.8 scroll delta to native coordinates, and reports the first
movement-command divergence.

## Remaining coordinate work

The original rider X, scroll accumulator, and native world X are now related,
but hoop/object coordinates and camera activation have not yet been decoded
into the same model. Those are intentionally left untouched until the next
first-divergence pass.
