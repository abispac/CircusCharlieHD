# Faithful event roadmap

This roadmap preserves the arcade game's defining roles, event order, hazards,
scroll behavior, controls, scoring opportunities, and difficulty escalation.
All production art, animation, audio, and source code will be newly authored.

## Event 1 — Fire rings

- Charlie is visibly a clown and rides a lion.
- Two-way horizontal control produces real forward/backward movement.
- Backtracking keeps Charlie and the lion facing forward, matching the arcade
  reverse-travel pose instead of flipping the artwork.
- One jump button controls the lion's leap.
- Fire rings hang from moving hardware connected to the ceiling tube/track.
- Large and small ring openings require distinct jump timing.
- Fire pots interrupt the ring sequence.
- Small rings contain increasingly valuable money bags.
- Backward-jump secrets and the no-miss reward are represented.
- A backwards hoop jump can replace the next ring with an extra-life doll.
- A backwards fire-pot jump can reveal the stage's hidden coin.
- Collecting all bags and the hidden coin without a miss triggers the goal
  bird and bonus-coin shower.
- Scrolling, meter markers, bonus countdown, and goal arrival follow the
  arcade event's cadence.

## Event 2 — Tightrope monkeys

- Charlie walks the tightrope rather than riding an animal.
- Left/right moves Charlie along the rope; the action button jumps.
- Monkeys approach in the opposite direction.
- Purple monkeys leapfrog the standard monkeys.
- Single- and double-monkey jumps have different risk and score values.
- Clearing both monkey colors in one jump awards the double-jump bonus.
- The goal platform hides the far-right money-bag route.
- The rope, tent, crowd, goal hook, and distance markers remain readable in
  both HD and 240p output.

## Event 3 — Trampolines

- Horizontal movement happens through timed trampoline bounces.
- The action button is not used in this event; left/right directs Charlie
  toward the next landing while the bounce cycle supplies vertical motion.
- Repeated bounces on one trampoline increase height.
- A trampoline permits three bounces; the fourth launches Charlie into the
  tent roof hazard.
- Fire breathers and sword/knife jugglers keep their distinct timing.
- Water variants replace ground hazards with dolphins.
- The water/dolphin variant follows the second, fourth, and sixth trampoline
  waves.
- The tent roof remains a real upper hazard.

## Event 4 — Rolling balls

- Charlie balances on moving circus balls.
- Left/right input changes the ball's travel.
- The action button jumps from the current ball.
- The player jumps between balls without touching the ground.
- Ball spacing and relative velocity determine landing difficulty.
- Skipping the next ball and landing on the third awards bonus points.
- Touching two balls together causes Charlie to lose balance.

## Event 5 — Pony and springboards

- The pony advances automatically.
- Left/right input controls pace.
- The action button launches Charlie from the pony.
- Charlie jumps from the pony across springboards and returns to the saddle.
- Springboard size, height, and spacing escalate through the event.
- Repeated bounces multiply each springboard's value.
- High platforms can be ridden under, while low platforms strike Charlie.
- Forward inertia continues after Charlie leaves the pony.

## Event 6 — Flying trapeze

- Pendulums use deterministic fixed-step motion.
- Charlie releases from one trapeze and catches the next.
- Left/right influences momentum; the action button releases the current bar.
- Lower platforms provide the recognizable recovery bounce.
- Skipping a trapeze and using recovery trampolines award distinct bonuses.
- The finish platform includes the original celebratory performer sequence.
- Swing timing and scroll position remain stable across every output refresh.

## Shared arcade systems

- Original portrait orientation.
- Fixed `60.606060... Hz` simulation.
- Two-way joystick plus one action button.
- Alternating two-player support.
- Lives, bonus timer, score, high score, credits, distance, and stage/wave.
- Selectable-event and linear-progression rulesets.
- Difficulty waves that change speed and obstacle timing without changing
  core input response.

## HD arcade presentation

- Recreate the complete boot and title presentation with newly drawn HD art.
- Include the multicolor star transitions between the title, demos, and
  ranking screens.
- Recreate the score-ranking/high-score screen and initials flow.
- Add a deterministic attract-mode demo that demonstrates multiple events.
- Preserve the selectable-event introduction and event difficulty grid.
- Include the original-style crash, game-over, event-clear, and celebration
  pacing without copying ROM graphics or audio.
