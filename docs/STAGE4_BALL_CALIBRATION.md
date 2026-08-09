# Event 4 rolling-ball calibration

This implementation is measured from the supplied `circusc4` Stage 4 frames
and the separate `charlie-falling` recording. ROM material remains a private
behavioral reference and is not copied into the project.

## Confirmed arcade behavior

- Charlie stands centered over the active ball with both arms extended.
- Left/right changes the active ball's travel; Charlie does not walk on the
  grass.
- Jump transfers Charlie between balls. Landing is resolved against the ball
  center and top surface rather than the ground.
- A normal jump may land on the next ball. Clearing one ball and landing on
  the following ball awards the skip bonus.
- Ball spacing and relative velocity remain live during the jump.
- Two balls touching triggers the balance failure.
- The failure is not a tumbling fall: Charlie is compressed beneath the ball,
  the ball remains above him, and the crowd's `OH NO!!` response begins while
  he is pinned.
- The goal is the wider striped leather platform used by the arcade recording.

## Native-frame measurements

- Source frame: 224 by 256 upright pixels.
- Standard ball diameter: approximately 17 source pixels.
- Charlie standing height above the ball: approximately 26 source pixels.
- Charlie's feet remain within roughly 2 source pixels of the ball center.
- The player anchor remains close to the left sixth of the playfield while
  the course scrolls.
- Ball clusters tighten toward the goal; the final platform remains fixed on
  the right during the approach.

## Runtime invariants

- Ball position, rotation, and velocity are fixed-step state.
- Charlie's grounded state means attached to a ball, never attached to grass.
- Every landing chooses the closest eligible ball under Charlie and snaps to
  its measured top-center anchor.
- Collision uses ball circles and a smaller Charlie foot/contact region.
- A missed landing and a ball-to-ball squeeze use separate failure triggers.
- Capture scenes must cover balance, transfer, skip, squeeze, fall, and goal.

## WAM motion contract

`wam-proof/stage4-charlie.wam` contains three authored motions:

- `ball_balance`: arms-out counterbalance with alternating feet.
- `ball_jump`: arms remain wide while the legs cycle through takeoff, apex,
  and landing.
- `ball_fall`: the measured compact/pinned failure, not a generic tumble.

The WAM strips are pose and timing references for the final HD raster atlases.
