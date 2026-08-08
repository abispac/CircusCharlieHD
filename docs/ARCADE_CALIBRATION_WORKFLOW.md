# Arcade calibration workflow

Every event is built from corrected MAME reference frames before gameplay is
tuned. The reference and remake are rendered side by side at the same logical
`480 x 640` size with a ten-by-ten grid. This prevents apparent size changes
caused by screenshots, window scaling, or the original board's non-square
pixels.

## Required order for each event

1. Capture representative MAME frames: start, normal play, every obstacle,
   collision, bonus, goal, and transition.
2. Correct each `224 x 256` board frame to `480 x 640`.
3. Lock the fixed marquee, HUD, playfield bands, rail, ground contact, and
   player bounding box.
4. Measure every object relative to the player in the same frame. This
   includes hoops, trampolines, balls, springs, monkeys, horses, trapezes,
   platforms, prizes, and score markers.
5. Render a deterministic remake capture with the same objects visible.
6. Place the original and remake captures side by side with the same grid.
7. Correct visible artwork first. Texture files may contain transparent
   padding, so final rendered pixels—not destination rectangles—are measured.
8. Derive collision shapes from the visible dangerous or collectible area.
   Hitboxes are never used to compensate for incorrectly scaled artwork.
9. Tune motion from frame counts and board displacement only after geometry
   matches.
10. Keep the comparison image in `docs/` so later changes can be checked for
    regressions.

## Stage 1 prize hoop example

The original reference is frame 800 of `hoop-extra.avi`. At corrected display
size, the visible prize hoop is approximately `50 x 110`, centered about 170
logical units above the lion's ground contact. Its visible white moneybag is
roughly `24 x 34`. The HD source cell has transparent padding on both axes, so its draw
rectangle must be wider and taller than the visible oval.

Generate the deterministic remake reference with:

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
./build/big_top_run --capture /private/tmp/stage1-prize.png --capture-scene prize
```

The checked comparison is `docs/stage1-prize-calibration.png`.
