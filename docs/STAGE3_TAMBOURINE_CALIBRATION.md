# Event 3 — Tambourine calibration

## Reference authority

- Frame sequence: `circusc4-frames/Stage 3 tamborines`
- Over-jump recording: `snap/charlie-overjumping.avi`
- Music: `Private Reference Audio/06 Trombonanza.mp3`
- Bounce effect: isolated sound command `0x46` (catalog RMS 1279)
- Money bag: isolated sound command `0x49` (catalog RMS 1567)
- Over-jump: isolated miss command `0x4f` (catalog RMS 1239)

The ROM/frame reference remains authoritative for scale, timing, obstacle
direction, and character identity. HD rendering may add intermediate frames,
but may not change the gameplay silhouette or timing rule.

## Bounce chain

Charlie continuously rebounds when he lands on a tambourine. Consecutive
landings advance a four-part chain:

1. Low bounce.
2. Medium bounce.
3. High bounce; this is the first arc that can reach a money bag.
4. Over-jump with the tuck/rotation sprite family, then the chain resets.

The drums are spaced one full normal forward-bounce apart. Missing the next
drum is a crash; the grass is not a safe landing surface.

## Stage objects

- Tambourines are tall padded leather cylinders with a white top, yellow rim,
  red triangles, and alternating pink/white vertical panels. They must never
  be flattened into an Event 1 goal platform.
- Knife throwers are French-looking adult showmen with a small hat and a large
  curled moustache.
- Fire throwers are bald, stocky adult showmen in purple and green.
- Both performers toss their hazards vertically. Projectiles rise and fall in
  the performer's fixed horizontal lane; they do not aim horizontally at the
  player.

## HD sprite assets

- `assets/stage3-charlie-bounce-12-v1.png`: 4x3 Charlie bounce/over-jump atlas.
- `assets/stage3-tambourine-v1.png`: tall leather tambourine.
- `assets/stage3-knife-thrower-8-v1.png`: 4x2 knife-thrower atlas.
- `assets/stage3-fire-breather-8-v1.png`: 4x2 fire-thrower atlas selected by
  the project owner.
- `assets/stage3-projectiles-8-v1.png`: four knife rotations and four fireball
  animation frames.

Original chroma-key generations remain under `assets/source-art/`; runtime
assets have transparent alpha and despilled edges.
