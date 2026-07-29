# Event 1 behavior reference

This file separates measured arcade behavior from tribute implementation
requirements supplied through hands-on play knowledge.

## Measured layout and sequence

- All large fire rings use the same proven opening and collision geometry.
- The first two large rings are separated by approximately `222` upright
  source pixels in the deterministic attract trace, or approximately `476`
  logical pixels in the HD course.
- The full recorded run proves that Event 1 does not repeat one simple
  two-large/one-small pattern. It progresses from two introductory large
  rings, through alternating moving rings and floor fires, to a close
  double-ring test and a denser final 10M gauntlet.
- The small ring center is approximately `62–68` source pixels above the
  rider's floor contact. The HD center is therefore `182` logical pixels above
  `kGroundY`, instead of the lower prototype placement.
- Distance is communicated by physical `60M`, `50M`, `40M`, `30M`, `20M`,
  and `10M` floor signs. It is not a HUD field.
- The goal is a striped pedestal with a `GOAL` plaque. Charlie stops on it
  while the crowd flashes `GREAT` and `FAROUT`.

## Hidden and random bonuses

- A small ring is always passable, but its prize contents are randomized.
  Its opening is sized around the complete lion-and-rider silhouette. Passing
  through removes the prize only; the ring itself remains visible and keeps
  moving on the overhead rail. A run must contain at least one prize ring and
  at least one empty ring.
- The small ring is still a fire obstacle. Its narrow center-plane collision
  burns the rider when the airborne lion is below or above the opening.
  Running beneath a small ring without jumping is safe. At the fixed jump apex
  the rider fits inside a forgiving `144`-logical-unit vertical opening. The
  ring is centered on the complete airborne sprite, and only its lower near
  arc receives a foreground render pass so the traversal is visually clear.
- Exactly one fire pot is selected as the hidden-coin pot for each run.
- The hidden coin can trigger only when Charlie returns to that pot and jumps
  over it while moving backward. A normal forward jump never reveals it.
- The coin follows one short arc and disappears after landing. Catching it
  awards bonus points, and the event can never spawn a second hidden coin.

## Perfect Event 1 reward

If the player collects every money bag that actually appeared in the run
without losing a life, a small bird must enter carrying a money bag, tear the
bag, and release a shower of falling coins that award additional points.
Empty small rings do not count against the perfect-clear condition.

The perfect-clear condition, bird entrance, bag-tear animation, coin shower,
and scoring sequence must be implemented as one presentation sequence rather
than by putting a permanent money bag inside every small ring.

## Finish and time tally

- The remaining bonus freezes when Charlie reaches the goal.
- The perfect-clear bird sequence, when earned, runs before the tally.
- The separate black `FINE!!` screen converts the remaining bonus counter
  with the recorded table:
  `6000–4500=10000`, `4499–4000=5000`, `3999–3500=4000`,
  `3499–3000=3000`, `2999–2500=2000`, `2499–2000=1000`,
  `1999–1500=800`, `1499–1001=600`, `1000–500=400`, and
  `499–0=200`.

## Crash presentation

- A fire collision first plays a dedicated burn beat with flames covering
  both Charlie and the lion.
- The retry panel is withheld until that animation completes, so the accident
  cannot be mistaken for either character simply disappearing.
- Local tracks 07 and 09 start together at the collision frame.
- Event 1 music stops at that same collision frame and restarts from its
  beginning only after Charlie respawns.
