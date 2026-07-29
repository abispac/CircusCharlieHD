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

## Random bonuses

- A small ring is always collectible, but its prize contents are randomized.
  A run must contain at least one prize ring and at least one empty ring.
- Each fire pot gets one random chance to release a single coin when Charlie
  jumps over it. The coin follows one short arc and disappears after landing.
  Catching it awards bonus points; a pot can never spawn repeatedly.

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
