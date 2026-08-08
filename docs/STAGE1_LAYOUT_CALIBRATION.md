# Stage 1 visual calibration

Stage 1 is calibrated by comparing a clean MAME frame and a deterministic
remake frame side by side at the same corrected `480 x 640` display size.
This avoids estimating positions from differently sized screenshots.

## Reference lines

- Fixed marquee: `y = 0–110`
- Score and lives panel: `y = 110–202`
- Black separation band: `y = 202–235`
- Top of crowd: `y = 235`
- Crowd plus grandstand fascia: `y = 235–350`
- Moving ring rail directly below the crowd fascia: `y = 350`
- Uninterrupted grass: `y = 350–640`
- Grounded lion contact: `y = 590`
- Grounded rider/lion visible size: approximately `100 x 70`
- Stage 1 obstacle collision center: rider world anchor `+20` logical units

The HD arena source is rendered as two independently scaled slices. The crowd
and grandstand fascia are compressed into their arcade-height band, while the
grass slice fills the remainder of the screen. The source painting's lower
curtain-and-stars foreground is excluded from Stage 1. The grass crop ends at
`85%` of the source height, before the old curtain's gold finials, so no
decorative fragments leak through the bottom edge.

Use this command after building to produce a clean remake reference frame:

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
./build/big_top_run --capture /private/tmp/stage1-layout.png --capture-scene layout
```
