# Stage 3 Charlie — WAM animation plan

## Reference inventory

- [G] Oversized round childlike head and short body; Charlie must read as the
  same friendly young clown at game size.
- [G] Long blue striped sleeping cap with three colored pom-poms.
- [G] Large round red nose and visible blond fringe.
- [G] Oversized curled blue-and-yellow clown shoes.
- [G] White gloves whose open-arm poses clearly change the silhouette.
- [C] Deep red one-piece costume with a darker red upper body.
- [C] Blue cap, yellow/blond hair, white gloves, red nose, blue cheek dots.
- [C] Rainbow belt separating torso and trousers.
- [-] Individual stitches, buttons, and tiny fabric seams are below the
  32-pixel silhouette threshold; they belong in final Blender materials.

## Motion contract

- `vertical_bounce`: front-facing compression, extension, apex hang, and
  recovery over one 48-board-frame bounce.
- `transfer_spin`: one continuous tucked rotation over the same 48 frames;
  the game, not the sprite, owns the center-to-center parabolic trajectory.
- `celebrate`: planted shoes with a readable crouch-rise-cheer loop.
- Gameplay timing stays ROM-derived. WAM supplies continuous rigged poses and
  evenly sampled in-betweens; it does not decide collisions or movement.

## Acceptance checks

- Silhouette remains recognizable at 24 and 32 pixels.
- Both shoe bottoms share a stable ground anchor in every celebration frame.
- Transfer frames contain no translation; swapping animation cannot create a
  horizontal or vertical gameplay jump.
- Compiler warnings are reviewed, not ignored, before any render is used.
