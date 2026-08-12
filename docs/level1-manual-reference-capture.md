# Level 1 manual reference capture

## Scope

This workflow records an unmodified, manually played `circusc4` Level 1 run.
It does not supply inputs, write emulated memory, alter gameplay, or touch the
native game and HD artwork.

The capture is intended to resolve only:

- goal platform and finish presentation
- hanging extra Charlie
- hidden fire-pot coin
- still-unmapped Level 1 score and bonus graphics

## What is recorded

`tools/capture_level1_manual_reference.lua` records every emulated frame:

- both 64-entry buffered sprite-RAM banks at `$3800-$39ff`
- every 16-byte object record at `$2400-$27ff`
- rider, jump, scroll, activation, course, score, and `$26d0` state fields
- optional new sprite code/attribute layouts as lossless screenshots
- one exact screenshot and RAM snapshot for each manual marker
- work RAM, color RAM, video RAM, and both sprite banks for each burst frame

Coordinates are not excluded from the trace. Automatic screenshot signatures
ignore coordinates so an object moving by one pixel does not create a new PNG
on every frame.

## Marker keys

The script reads these keys without forwarding or synthesizing game input:

| Key | Marker |
| --- | --- |
| `E` | hanging extra Charlie |
| `Up Arrow` | hidden coin |
| `G` | goal platform or finish presentation |
| `B` | score or bonus graphic |
| `M` | other useful Level 1 object/state |
| `X` | finalize the capture files |

Keys are edge-triggered. Release and press a key again to add another marker
of the same type. Each press captures that exact frame. Press the marker more
than once while an object animates to preserve additional poses. Continuous
sprite and object traces preserve the frames between markers.

Automatic screenshots are disabled by default because repeated lossless PNG
writes can interrupt real-time audio and gameplay on macOS. They can be
enabled deliberately with `CIRCUS_MANUAL_AUTO_LIMIT`, but are not needed for
this guided marker pass.

## Priority-aware extra-Charlie verification

The capture stores all hardware slots in both sprite banks, not only the four
candidate extra-Charlie cells. Reconstruction will select the bank that
matches the exact screenshot, compose the candidate from its actual slots,
and then simulate MAME's ascending-slot sprite draw order.

Every candidate pixel not covered by a later genuine sprite must match the
MAME framebuffer exactly. Pixels covered by a later sprite are retained in
the exported hardware composite but excluded from the framebuffer mismatch
count. Metadata will list their coordinates, the covering slot and sprite
code, and the observed frame. This is an occlusion mask derived from hardware
slots, not a manual pixel repair.

## Manual play sequence

1. Launch MAME with the command provided in the handoff.
2. Insert a coin, start the game, select Level 1, and play manually.
3. Trigger the legitimate hanging extra-Charlie presentation. Press `E` while
   the complete hanging Charlie is visible. Press it again at a second clear
   animation pose if practical.
4. Trigger the legitimate hidden coin from a fire pot. Press `Up Arrow` once
   while it is rising and again near its apex or return to the pot.
5. Continue to the Level 1 goal. Press `G` when the goal platform is fully
   visible, when Charlie lands, and during each distinct finish presentation.
6. Press `B` whenever an unmapped score/bonus graphic is clearly visible,
   including the final bonus calculation. Use `M` for any other unmapped
   Level 1 object worth preserving.
7. After the finish and bonus presentation is over, wait two seconds, press
   `X`, and wait for `REFERENCE CAPTURE COMPLETE`.
8. Quit MAME normally.

No exact joystick timing is prescribed by this workflow. The player supplies
all legitimate inputs manually.

## Completion rule

The session is complete only when the capture contains at least one `E`,
`Up Arrow`, `G`, and `B` marker, the finish/bonus presentation has completed,
and pressing `X` has created `capture-complete.txt`. The checker reports
whether those required files, marker labels, and screenshots are present.

After a successful capture, the existing ROM/PROM decoder and composite
pipeline can identify actual slots/cells and export only hardware-backed
objects that pass exact or priority-masked zero-mismatch verification. Any
object that cannot be identified confidently remains unresolved.
