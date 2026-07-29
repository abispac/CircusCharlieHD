# Faithful event roadmap

This roadmap preserves the arcade game's defining roles, event order, hazards,
scroll behavior, controls, scoring opportunities, and difficulty escalation.
All production art, animation, audio, and source code will be newly authored.

## Event 1 — Fire rings

- Charlie is visibly a clown and rides a lion.
- Two-way horizontal control changes forward/backward movement.
- One jump button controls the lion's leap.
- Fire rings hang from moving hardware connected to the ceiling tube/track.
- Large and small ring openings require distinct jump timing.
- Fire pots interrupt the ring sequence.
- Money bags occupy risky routes.
- Backward-jump secrets and the no-miss reward are represented.
- Scrolling, meter markers, bonus countdown, and goal arrival follow the
  arcade event's cadence.

## Event 2 — Tightrope monkeys

- Charlie walks the tightrope rather than riding an animal.
- Monkeys approach in the opposite direction.
- Purple monkeys leapfrog the standard monkeys.
- Single- and double-monkey jumps have different risk and score values.
- The rope, tent, crowd, goal hook, and distance markers remain readable in
  both HD and 240p output.

## Event 3 — Trampolines

- Horizontal movement happens through timed trampoline bounces.
- Repeated bounces on one trampoline increase height.
- Fire breathers and sword/knife jugglers keep their distinct timing.
- Water variants replace ground hazards with dolphins.
- The tent roof remains a real upper hazard.

## Event 4 — Rolling balls

- Charlie balances on moving circus balls.
- Left/right input changes the ball's travel.
- The player jumps between balls without touching the ground.
- Ball spacing and relative velocity determine landing difficulty.

## Event 5 — Pony and springboards

- The pony advances automatically.
- Left/right input controls pace.
- Charlie jumps from the pony across springboards and returns to the saddle.
- Springboard size, height, and spacing escalate through the event.

## Event 6 — Flying trapeze

- Pendulums use deterministic fixed-step motion.
- Charlie releases from one trapeze and catches the next.
- Lower platforms provide the recognizable recovery bounce.
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
