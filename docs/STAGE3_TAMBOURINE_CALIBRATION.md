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
4. A final front-facing vertical rebound that strikes the tent roof and costs
   a life if Charlie has remained on the same tambourine.

Pressing left or right during the opening eight board frames commits Charlie
to the adjacent tambourine. The stick must return to neutral before another
transfer can be selected, so holding one direction cannot auto-clear the
course. Every transfer uses one full 48-frame cosine-eased arc and lands at the
exact center of the selected drum; horizontal free drift does not exist. A
successful transfer resets the stationary chain to its first, low bounce. The
grass is never a landing surface, and performers are not platforms.

Measured regular drum centers are approximately 84–88 pixels apart in the
224-pixel ROM frame. The HD course uses a 180-unit center spacing on its
480-unit logical canvas, with performers centered in the intended gaps.

Every landing compresses the tambourine artwork before it recovers. Only a
knife/flame collision, the fourth stationary roof impact, or an expired timer
can kill Charlie. The final target is a separate double-width goal tambourine.

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
- Hazard clocks run continuously whether or not the performer is visible.
  Lanes start at deterministic staggered phases. Knife flights occupy 108
  board frames of a 150-frame cycle; flame droplets rise once for 46 frames,
  disappear, and leave the remaining 104 frames clear. This preserves the
  arcade's readable repeating lanes instead of spawning hazards in response
  to the camera or the player.

## HD sprite assets

- `assets/stage3-charlie-vertical-front-12-v2.png`: 4x3 front-facing vertical
  bounce atlas.
- `assets/stage3-charlie-bounce-12-v1.png`: tucked center-to-center rotation
  atlas retained as the original visual reference.
- `wam-proof/stage3-charlie.wam`: compiled WAM rig and the authoritative
  vertical-bounce, transfer-tuck, and celebration motion contract.
- `assets/stage3-charlie-wam-tuck-hd-v1.png`: centered HD tuck pose derived
  from that WAM rig. Runtime rotation is continuous, so the 48-frame transfer
  has no atlas-frame snapping.
- `assets/stage3-tambourine-v1.png`: tall leather tambourine.
- `assets/stage3-goal-tambourine-v1.png`: double-width final tambourine.
- `assets/stage3-knife-thrower-8-v1.png`: 4x2 knife-thrower atlas.
- `assets/stage3-fire-breather-vertical-8-v2.png`: 4x2 vertical fire-thrower
  atlas selected by the project owner.
- `assets/stage3-projectiles-8-v1.png`: four rotating knife frames.
- `assets/stage3-flame-projectile-4-v2.png`: four upright HD teardrop flame
  frames; the projectile never rotates or develops a sideways tail.

Original chroma-key generations remain under `assets/source-art/`; runtime
assets have transparent alpha and despilled edges.
