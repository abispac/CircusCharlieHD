# Stage 1 WAM proof

This experiment tests WAM as the rigging and base-animation layer for the
Stage 1 lion. It does not replace the active game sprite sheet.

The source reference is `../assets/stage1-rider-sheet-v7.png`. WAM generates
the skinned animated glTF; Blender remains responsible for the final HD fur,
materials, lighting, Charlie, saddle, fixed camera, and transparent sprite
render.

Acceptance criteria:

- The side silhouette reads as the same lion at game size.
- The tail stays behind the body throughout the full gait.
- All four paws follow a believable 12-frame arcade walk without moonwalking.
- The body and head bob subtly without the jump-like shake of the cutout rig.
- The glTF imports into Blender with the skeleton and animation intact.
- A fixed-camera transparent render can replace one version 7 frame without
  changing game scale or collision geometry.

The WAM compiler is intentionally kept outside this repository until the
proof passes. Its upstream source is https://github.com/elliottdehn/wam and
its license is MIT. This proof was compiled against upstream commit
`71916323ab9c7e8ba53e3b387df52ec13246a7bf`.

Current result:

- WAM compilation passes and emits a 1,760-triangle animated lion.
- The 12-frame `arcade_walk` animation exports correctly to glTF.
- Blender 5.2 imports the mesh, skeleton, materials, and animation.
- Version 7 remains untouched and is still the active visual benchmark.
- The raw generated anatomy is a blockout, not final HD production art.
- The remaining rear-leg self-intersection warning must be fixed before the
  rig can be considered production-safe.

The next production step is to use the WAM skeleton and timing underneath a
properly sculpted Blender lion, rather than trying to polish WAM's procedural
body into the final character.
