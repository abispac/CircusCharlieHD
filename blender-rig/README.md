# Stage 1 Blender rider rig

This folder contains the editable Stage 1 Charlie-and-lion animation source.
It uses a locked orthographic camera and articulated 2.5D planes so every
rendered frame keeps the same character scale and ground anchor.

## Files

- `stage1-rider-rig.blend` is the editable Blender 5.2 rig with packed images.
- `stage1_rider_rig.py` reconstructs the rig and renders all six poses.
- `../assets/source-art/` contains the versioned lion and Charlie texture
  atlases, including their transparent despilled versions.
- `../assets/stage1-rider-sheet-v8.png` is the production 3-by-2 sheet used by
  the native renderer.
- `stage1-build/` is a disposable ignored render cache.

## Pose map

1. Ground stride contact.
2. Ground stride extension.
3. Ground stride passing pose.
4. Compressed takeoff/landing pose.
5. Stable airborne pose used for the fixed arcade jump.
6. Grounded recovery pose.

Charlie remains attached to the saddle and leans toward the mane in every
pose. The lion tail is the rearmost render layer and can never appear in front
of the body.

## Rebuild

```sh
cd "/Users/abispac/AppDev/Circus Charlie/BigTopRunNative"
/Applications/Blender.app/Contents/MacOS/Blender --background --python blender-rig/stage1_rider_rig.py
cmake --build build
```

The script produces the editable `.blend`, six individual transparent frames,
and the final versioned sheet. It never overwrites the older `v7` source.

## Source-art prompts

The lion atlas was generated as a clean exploded puppet-parts sheet matching
the existing golden adult male circus lion. It requested separate head/mane,
torso, rump, tail, four leg poses, and saddle on a flat green background, with
realistic fur, consistent side-profile lighting, no overlaps, and no text.

The Charlie atlas was generated as a matching exploded puppet-parts sheet. It
requested separate head/hat, jacket torso, seated hips, two forward gripping
arms, two bent riding legs, and belt on a flat green background, with the same
blond clown identity, red costume, blue-striped hat, and curled shoes.

Both atlases used the built-in image-generation workflow. Their green matte
was converted to alpha with the standard soft-matte and despill helper before
Blender imported the parts.
