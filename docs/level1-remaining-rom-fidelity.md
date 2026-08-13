# Level 1 remaining ROM-fidelity passes

This work preserves the accepted Level 1 movement, fixed jump samples,
large-hoop collision, large-hoop scheduler, camera/course behavior, rider
artwork/scale, landing, scoring, and Events 2–4.

## Airborne direction locking

### Original mechanism

`$7344` reloads the active movement command `<$b1:$b2` from the takeoff copy
at `$2243:$2244` while jump state `<$b0` is nonzero. `$736c-$7391` starts the
jump; `$7257-$727f` restores grounded control on landing. Direction is thus
latched at takeoff. Releasing or pressing the opposite direction is sampled,
but cannot replace the airborne command. A neutral takeoff remains neutral.

### Evidence

`tools/trace_level1_airborne_direction.lua` traces right-held, right-to-left,
right-release, left-to-right, left-release, and neutral takeoffs. Right jumps
hold `fe80`; left jumps hold `0130`; neutral jumps hold `0000` through the
complete airborne interval. Live input resumes on the landing transition.

### Native correction and regression

`Player::level1AirborneDirection` stores the takeoff direction. Level 1 uses
that value only while airborne and restores live input when grounded. The six
native deterministic modes reproduce the traced behavior. The synchronized
successful-large-hoop comparison remains exact through its full window.

## Hoop visual priority and layering

### Original mechanism

The rider occupies hardware sprite slots 31–36. The three-cell large hoop
occupies slots 45–47. Hardware draws the complete hoop after the rider; there
is no crossing-time slot reassignment and no priority-state transition. The
hoop artwork's transparent center exposes the rider while the flame rim stays
in front, creating the through-the-hoop illusion.

### Native correction and regression

Native already staged the hanger before the rider and deferred the transparent
flame rim until after the rider. The misleading category-tile explanation was
replaced with the verified fixed hardware-slot rule. Collision and artwork
were unchanged.

## Small/large hoop course scheduling

### Original mechanism

The four reusable large-hoop records remain `$26d0`, `$2700`, `$2730`, and
`$2760`, admitted by `$7607-$767e` using the ten course reload bytes:

`dc f4 d4 ec dc e4 de 1b ee f6`

`$7539-$7554` advances each active large hoop in 8.8 fixed point. During that
same active-object pass, `$75eb-$7606` stages the associated small/prize ring
from the large hoop's **integer** X byte minus 16 source pixels. The small ring
does not have an independent authored position or fixed-point accumulator.

The course trace proves the general pairing:

- first admitted large hoop `$26d0`: large `ff00`, small `$2400=ef00`;
- later large hoop `$26d0=6280`, small `$2400=5200`;
- second admitted slot `$2700=fd80`, small `$2430=ed00`.

Thus every admitted large-hoop slot owns one associated small/prize record.
The ROM can have multiple live large hoops, and therefore multiple associated
small rings, but it cannot produce the old native combinations caused by five
independently authored small-ring world positions.

The collision scan at `$70dc-$71c8` tests large hoops `$26d0-$2760` and floor
objects `$24b0-$2570`; it does not collision-test associated small-ring cells
`$2400-$24bf`. The old reconstructed small-ring flame hitbox caused a failure
inside the MAME-verified successful-hoop sequence as soon as exact pairing was
enabled, so that non-ROM hazard test was removed while prize collection at the
small-ring center was retained.

### Shared extra-Charlie entitlement

The opening three backward jumps and a backward crossing of an already-cleared
large hoop now call the same Stage 1 entitlement routine. The first trigger
consumes the entitlement immediately, even if the reward must wait for the
next reusable hoop admission. Collection awards one life. Neither a later
reverse-hoop crossing nor another opening sequence can arm a second reward,
and `restartAfterCrash()` deliberately preserves the consumed flag. Only a
new Stage 1 course resets it.

### Native correction and regression

Native now creates four associated ring records, updates each from its large
hoop's integer X, retires/reuses the pair together, and no longer uses
`railStartForIntercept()` for small rings. The synchronized successful-hoop
test again passes every compared frame after the non-ROM small-ring collision
test was removed. Fire-pot scheduling was not altered by this scoped pass.

## Course-start reverse movement

### Original mechanism

At the start line, LEFT is accepted and writes movement command `0130`, but
course scroll `$2203:$2204` is still zero. Both `$7539` and `$7607` test the
high byte `$2203`; while it is zero they replace the scroll contribution with
`0000`, then subtract the intrinsic rail term `$0080`. Live hoops therefore
continue left by `ff80` per board frame instead of moving right.

Once RIGHT creates forward course progress, normal reverse object movement is
restored: `0130-$0080 = 00b0`. The MAME `right-left` and
`right-return-left` traces show that this is a course-start boundary state,
not a global ban on reverse movement.

### Native correction and regression

Native records whether Level 1 has acquired forward progress. Before that
state, a left command contributes zero to the object scheduler while player
input remains accepted; after it, LEFT again contributes `00b0`. The aligned
neutral and hold-left traces match MAME hoop motion for all 120 compared
frames, and the per-frame right-input delta is the same `fe00` in both. The
successful-large-hoop synchronization test remains exact.

## Verification summary

- Native build: passed.
- Six airborne direction-lock traces: passed.
- Start neutral and initial hold-left: 120 aligned frames each.
- Forward object delta: MAME/native both `fe00` for the 59-frame stable
  comparison window before the original trace changes scene/state.
- Successful large-hoop comparator: synchronized through the full comparison
  window after all four passes.
- No rider/lion artwork, physics, jump table, large-hoop collision constants,
  camera constants, or Events 2–4 were changed.
