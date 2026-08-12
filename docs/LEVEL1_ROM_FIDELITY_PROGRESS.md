# Level 1 ROM-fidelity progress

## Completed first task

- Preserved the prior playable checkpoint at commit `376b233` and branch
  `level1-rom-fidelity`.
- Added reproducible MAME traces for hold RIGHT, RIGHT/release, RIGHT/LEFT,
  and a full-speed forward jump.
- Added deterministic native CSV tracing for the same four sequences.
- Removed Level 1's exponential movement response and replaced it with the
  ROM's immediate signed fixed-point direction commands.
- Did not change obstacle coordinates, collision dimensions, artwork, or
  Levels 2 through 4.

## Measurements

- RIGHT: `FE80`, or -1.5 original scroll pixels per frame.
- LEFT: `0130`, or +1.1875 original scroll pixels per frame.
- release: `0000` on the first released sample.
- reversal: direct `FE80` to `0130`, without an intermediate ramp.
- forward jump: continues using `FE80` throughout takeoff.

Converted to the 480-wide native logical canvas at 60.606060 Hz, these are
`+194.805176` and `-154.220779` units/second. Native deterministic traces show
`+3.214286`, `0`, and `-2.544643` units per board frame as appropriate.

## Changed code

- `src/main.cpp`: ROM-derived Level 1 speeds, immediate direction response,
  and optional deterministic native trace output.
- `tools/trace_level1_movement.lua`: MAME input/RAM/object/write tracing.
- `tools/compare_level1_movement.py`: automated first-divergence comparison.
- `docs/level1-original-coordinate-model.md`: source coordinate/timing model.

## Remaining discrepancies and next divergence

The horizontal movement command is synchronized for all four 120-frame test
windows. With the full-speed jump moved earlier to avoid the first immediate
obstacle, takeoff and the first 58 vertical samples also match. The next first
divergence is native trace frame 68 / MAME frame 1390: MAME's visible rider
slot stops at source Y 208 while native continues the fixed arc from logical
Y 550 to 557.5. That transition coincides with the approaching hoop/object
state and must be resolved by tracing all rider composite slots, hoop rail
position, collision state, and scroll state—not by tuning the jump table.

The six-slot/frame-1390 investigation is documented in
`docs/LEVEL1_FRAME_1390_ANALYSIS.md`. It proves that `$7130-$7192` enters the
failure routine at `$7c47` with active object pointer `U=$26d0`; this freezes
the prior buffered rider pose and changes its sprite codes. Native now
preserves that buffered vertical pose on hoop failure.

The expanded object-state comparison now exposes the earlier cause at native
frame 53 / MAME frame 1375: MAME's active hoop is `85.714286` native logical
units ahead of the rider axis, while native's nearest authored hoop is
`496.167358` units ahead. By frame 68 / MAME frame 1390, MAME has entered hoop
failure while native is still `Playing`. Resolving that requires the later,
separately scoped hoop object activation/position pass.
