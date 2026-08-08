"""Build and render the editable Stage 1 Charlie-and-lion sprite rig.

Run with Blender, not the system Python:

    /Applications/Blender.app/Contents/MacOS/Blender --background \
      --python blender-rig/stage1_rider_rig.py

The source paintings are split into articulated 2.5D planes.  The locked
orthographic camera and shared pose anchors prevent the scale and baseline
jitter that appeared in independently generated sprite frames.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from pathlib import Path
import math

import bpy


ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "assets" / "source-art"
BUILD_ROOT = ROOT / "blender-rig" / "stage1-build"
PARTS_ROOT = BUILD_ROOT / "parts"
FRAMES_ROOT = BUILD_ROOT / "frames"
BLEND_PATH = ROOT / "blender-rig" / "stage1-rider-rig.blend"
SHEET_PATH = ROOT / "assets" / "stage1-rider-sheet-v8.png"

CANVAS_SIZE = 512
FRAME_COUNT = 6


@dataclass(frozen=True)
class Crop:
    name: str
    source: str
    left: int
    top: int
    width: int
    height: int
    remove_gray: bool = False


CROPS = (
    Crop("lion_head", "stage1-lion-rig-atlas-v1-alpha.png", 80, 20, 430, 365),
    Crop("lion_body", "stage1-lion-rig-atlas-v1-alpha.png", 545, 60, 590, 325),
    Crop("lion_rear", "stage1-lion-rig-atlas-v1-alpha.png", 1245, 65, 295, 300),
    Crop("lion_tail", "stage1-lion-rig-atlas-v1-alpha.png", 330, 385, 700, 145),
    Crop("lion_front_leg_a", "stage1-lion-rig-atlas-v1-alpha.png", 155, 500, 230, 385),
    Crop("lion_front_leg_b", "stage1-lion-rig-atlas-v1-alpha.png", 440, 490, 390, 395),
    Crop("lion_rear_leg_a", "stage1-lion-rig-atlas-v1-alpha.png", 950, 485, 260, 415),
    Crop("lion_rear_leg_b", "stage1-lion-rig-atlas-v1-alpha.png", 1235, 545, 315, 345),
    Crop("saddle", "stage1-lion-rig-atlas-v1-alpha.png", 1150, 325, 420, 275),
    Crop("charlie_head", "stage1-charlie-rig-atlas-v1-alpha.png", 20, 55, 415, 325),
    Crop("charlie_torso", "stage1-charlie-rig-atlas-v1-alpha.png", 415, 115, 300, 325),
    Crop("charlie_arm_a", "stage1-charlie-rig-atlas-v1-alpha.png", 1080, 100, 385, 215),
    Crop("charlie_arm_b", "stage1-charlie-rig-atlas-v1-alpha.png", 1080, 375, 385, 220),
    Crop("charlie_hips", "stage1-charlie-rig-atlas-v1-alpha.png", 730, 135, 290, 305),
    Crop("charlie_leg_a", "stage1-charlie-rig-atlas-v1-alpha.png", 145, 555, 340, 390),
    Crop("charlie_leg_b", "stage1-charlie-rig-atlas-v1-alpha.png", 1050, 545, 330, 410),
)


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = CANVAS_SIZE
    scene.render.resolution_y = CANVAS_SIZE
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = True
    scene.render.image_settings.color_depth = "8"
    scene.render.fps = 60
    scene.frame_start = 1
    scene.frame_end = FRAME_COUNT
    scene.view_settings.look = "AgX - Medium High Contrast"


def image_pixel(pixels: list[float], width: int, height: int,
                x: int, y_from_top: int) -> tuple[float, float, float, float]:
    y = height - 1 - y_from_top
    index = (y * width + x) * 4
    return tuple(pixels[index:index + 4])


def is_background(pixel: tuple[float, float, float, float],
                  remove_gray: bool) -> bool:
    red, green, blue, alpha = pixel
    if alpha < 0.5:
        return True
    maximum = max(red, green, blue)
    minimum = min(red, green, blue)
    saturation = maximum - minimum
    if green > 0.55 and green > red * 1.45 and green > blue * 1.45:
        return True
    if maximum < 0.145:
        return True
    return remove_gray and saturation < 0.075 and maximum < 0.52


def is_chroma_green(pixel: tuple[float, float, float, float]) -> bool:
    red, green, blue, _ = pixel
    return green > 0.48 and green > red * 1.32 and green > blue * 1.32


def extract_crop(crop: Crop) -> Path:
    PARTS_ROOT.mkdir(parents=True, exist_ok=True)
    source_path = SOURCE_ROOT / crop.source
    source = bpy.data.images.load(str(source_path), check_existing=False)
    source_width, source_height = source.size
    pixels = list(source.pixels[:])

    if crop.left + crop.width > source_width or crop.top + crop.height > source_height:
        raise ValueError(f"Crop {crop.name} exceeds {source_path.name}")

    sampled = [
        [image_pixel(pixels, source_width, source_height,
                     crop.left + x, crop.top + y)
         for x in range(crop.width)]
        for y in range(crop.height)
    ]

    outside: set[tuple[int, int]] = set()
    queue: deque[tuple[int, int]] = deque()
    for x in range(crop.width):
        queue.append((x, 0))
        queue.append((x, crop.height - 1))
    for y in range(crop.height):
        queue.append((0, y))
        queue.append((crop.width - 1, y))

    while queue:
        x, y = queue.popleft()
        if (x, y) in outside or not (0 <= x < crop.width and 0 <= y < crop.height):
            continue
        if not is_background(sampled[y][x], crop.remove_gray):
            continue
        outside.add((x, y))
        queue.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))

    foreground: set[tuple[int, int]] = set()
    for y_from_top in range(crop.height):
        for x in range(crop.width):
            pixel = sampled[y_from_top][x]
            if ((x, y_from_top) not in outside and
                    not is_chroma_green(pixel) and pixel[3] > 0.02):
                foreground.add((x, y_from_top))

    largest_component: set[tuple[int, int]] = set()
    unvisited = set(foreground)
    while unvisited:
        start = unvisited.pop()
        component = {start}
        component_queue = deque([start])
        while component_queue:
            x, y = component_queue.popleft()
            for neighbor in ((x - 1, y), (x + 1, y),
                             (x, y - 1), (x, y + 1)):
                if neighbor in unvisited:
                    unvisited.remove(neighbor)
                    component.add(neighbor)
                    component_queue.append(neighbor)
        if len(component) > len(largest_component):
            largest_component = component

    output_pixels = [0.0] * (crop.width * crop.height * 4)
    for y_from_top in range(crop.height):
        output_y = crop.height - 1 - y_from_top
        for x in range(crop.width):
            red, green, blue, alpha = sampled[y_from_top][x]
            if (x, y_from_top) not in largest_component:
                alpha = 0.0
            output_index = (output_y * crop.width + x) * 4
            output_pixels[output_index:output_index + 4] = (
                red, green, blue, alpha
            )

    output_path = PARTS_ROOT / f"{crop.name}.png"
    result = bpy.data.images.new(crop.name, crop.width, crop.height, alpha=True)
    result.pixels.foreach_set(output_pixels)
    result.filepath_raw = str(output_path)
    result.file_format = "PNG"
    result.save()
    bpy.data.images.remove(source)
    bpy.data.images.remove(result)
    return output_path


def make_material(name: str, image_path: Path) -> bpy.types.Material:
    image = bpy.data.images.load(str(image_path), check_existing=True)
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    mix = nodes.new("ShaderNodeMixShader")
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    emission = nodes.new("ShaderNodeEmission")
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = image
    texture.interpolation = "Linear"

    links.new(texture.outputs["Color"], emission.inputs["Color"])
    emission.inputs["Strength"].default_value = 1.0
    links.new(texture.outputs["Alpha"], mix.inputs[0])
    links.new(transparent.outputs[0], mix.inputs[1])
    links.new(emission.outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], output.inputs[0])
    return material


def make_part(name: str, image_path: Path, scale: float,
              pivot_x: float, pivot_y: float, depth: float) -> bpy.types.Object:
    image = bpy.data.images.load(str(image_path), check_existing=True)
    width = image.size[0] * scale
    height = image.size[1] * scale
    left = -pivot_x * width
    right = (1.0 - pivot_x) * width
    bottom = -pivot_y * height
    top = (1.0 - pivot_y) * height
    mesh = bpy.data.meshes.new(f"{name}_mesh")
    mesh.from_pydata(
        [(left, bottom, depth), (right, bottom, depth),
         (right, top, depth), (left, top, depth)],
        [],
        [(0, 1, 2, 3)],
    )
    mesh.uv_layers.new(name="UVMap")
    uvs = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    for loop, uv in zip(mesh.uv_layers[0].data, uvs):
        loop.uv = uv
    part = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(part)
    part.data.materials.append(make_material(f"{name}_material", image_path))
    return part


def set_pose(part: bpy.types.Object, frame: int, x: float, y: float,
             angle: float = 0.0, scale_x: float = 1.0,
             scale_y: float = 1.0) -> None:
    part.location.x = x
    part.location.y = y
    part.rotation_euler.z = math.radians(angle)
    part.scale.x = scale_x
    part.scale.y = scale_y
    part.keyframe_insert("location", frame=frame)
    part.keyframe_insert("rotation_euler", frame=frame)
    part.keyframe_insert("scale", frame=frame)


def build_camera() -> None:
    camera_data = bpy.data.cameras.new("Stage1Camera")
    camera = bpy.data.objects.new("Stage1Camera", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (0.0, 0.0, 20.0)
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = float(CANVAS_SIZE)
    bpy.context.scene.camera = camera


def build_rig(paths: dict[str, Path]) -> dict[str, bpy.types.Object]:
    specs = {
        "lion_tail": (0.23, 0.94, 0.50, 0.00),
        "lion_rear_leg_a": (0.31, 0.50, 0.90, 0.01),
        "lion_rear_leg_b": (0.31, 0.50, 0.90, 0.02),
        "lion_front_leg_a": (0.31, 0.50, 0.90, 0.03),
        "lion_front_leg_b": (0.31, 0.50, 0.90, 0.04),
        "lion_rear": (0.40, 0.50, 0.50, 0.05),
        "lion_body": (0.43, 0.50, 0.50, 0.06),
        "lion_head": (0.42, 0.42, 0.48, 0.07),
        "saddle": (0.32, 0.50, 0.50, 0.08),
        "charlie_leg_a": (0.21, 0.50, 0.88, 0.09),
        "charlie_leg_b": (0.21, 0.50, 0.88, 0.10),
        "charlie_hips": (0.24, 0.50, 0.50, 0.11),
        "charlie_torso": (0.25, 0.50, 0.45, 0.12),
        "charlie_arm_b": (0.21, 0.10, 0.50, 0.13),
        "charlie_arm_a": (0.21, 0.10, 0.50, 0.14),
        "charlie_head": (0.27, 0.48, 0.42, 0.15),
    }
    return {
        name: make_part(name, paths[name], *spec)
        for name, spec in specs.items()
    }


def author_poses(parts: dict[str, bpy.types.Object]) -> None:
    # Each tuple is a genuine arcade-style silhouette anchor rather than an
    # independently scaled illustration.  Frame 5 is the stable jump image.
    stride = (
        {"body_y": -70, "body_angle": 0, "rear_a": -10, "rear_b": 17,
         "front_a": 12, "front_b": -14, "charlie": 0, "tail": -7},
        {"body_y": -67, "body_angle": -2, "rear_a": 26, "rear_b": -24,
         "front_a": -25, "front_b": 25, "charlie": -2, "tail": 5},
        {"body_y": -69, "body_angle": 1, "rear_a": -28, "rear_b": 22,
         "front_a": 28, "front_b": -22, "charlie": 1, "tail": 10},
        {"body_y": -76, "body_angle": -5, "rear_a": 42, "rear_b": 18,
         "front_a": -42, "front_b": -18, "charlie": -5, "tail": 14},
        {"body_y": -46, "body_angle": 0, "rear_a": 58, "rear_b": 39,
         "front_a": -53, "front_b": -34, "charlie": 0, "tail": 18},
        {"body_y": -72, "body_angle": 3, "rear_a": -22, "rear_b": 28,
         "front_a": 24, "front_b": -27, "charlie": 3, "tail": -3},
    )

    for frame, pose in enumerate(stride, start=1):
        body_y = pose["body_y"]
        body_angle = pose["body_angle"]
        airborne = frame == 5
        compression = frame == 4
        base_x = 4.0
        leg_y = body_y - 24.0
        rear_x = -82.0
        front_x = 72.0

        set_pose(parts["lion_tail"], frame, -103, body_y + 16,
                 pose["tail"], 1.0, 1.0)
        set_pose(parts["lion_rear_leg_a"], frame, rear_x - 12, leg_y,
                 pose["rear_a"], 0.92, 0.78 if airborne else 1.0)
        set_pose(parts["lion_rear_leg_b"], frame, rear_x + 16, leg_y + 1,
                 pose["rear_b"], 0.90, 0.76 if airborne else 1.0)
        set_pose(parts["lion_front_leg_a"], frame, front_x - 12, leg_y + 1,
                 pose["front_a"], 0.92, 0.76 if airborne else 1.0)
        set_pose(parts["lion_front_leg_b"], frame, front_x + 14, leg_y,
                 pose["front_b"], 0.90, 0.78 if airborne else 1.0)
        set_pose(parts["lion_rear"], frame, -102, body_y + 2,
                 body_angle, 1.0, 0.92 if compression else 1.0)
        set_pose(parts["lion_body"], frame, -5, body_y,
                 body_angle, 1.0, 0.92 if compression else 1.0)
        set_pose(parts["lion_head"], frame, 88, body_y + 31,
                 body_angle)
        set_pose(parts["saddle"], frame, -35, body_y + 53,
                 body_angle)

        rider_y = body_y + 67
        rider_angle = pose["charlie"]
        set_pose(parts["charlie_leg_b"], frame, -12, rider_y - 29,
                 -58 + rider_angle)
        set_pose(parts["charlie_leg_a"], frame, -33, rider_y - 24,
                 -44 + rider_angle)
        set_pose(parts["charlie_hips"], frame, -28, rider_y - 8,
                 rider_angle)
        set_pose(parts["charlie_torso"], frame, -24, rider_y + 24,
                 -8 + rider_angle)
        # Both hand-bearing arm layers end at the mane in all six frames.
        set_pose(parts["charlie_arm_b"], frame, -26, rider_y + 34,
                 -28 + rider_angle)
        set_pose(parts["charlie_arm_a"], frame, -20, rider_y + 39,
                 -23 + rider_angle)
        set_pose(parts["charlie_head"], frame, -17, rider_y + 67,
                 -7 + rider_angle)


def configure_render() -> None:
    scene = bpy.context.scene
    scene.render.filepath = str(FRAMES_ROOT / "rider-")
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = True
    scene.render.use_file_extension = True
    scene.render.resolution_x = CANVAS_SIZE
    scene.render.resolution_y = CANVAS_SIZE
    scene.render.resolution_percentage = 100
    if scene.world is None:
        scene.world = bpy.data.worlds.new("Stage1World")
    scene.world.color = (0.0, 0.0, 0.0)


def assemble_sheet() -> None:
    images = []
    for frame in range(1, FRAME_COUNT + 1):
        path = FRAMES_ROOT / f"rider-{frame:04d}.png"
        images.append(bpy.data.images.load(str(path), check_existing=False))
    output = bpy.data.images.new(
        "stage1-rider-sheet-v8", CANVAS_SIZE * 3, CANVAS_SIZE * 2, alpha=True
    )
    sheet_pixels = [0.0] * (CANVAS_SIZE * 3 * CANVAS_SIZE * 2 * 4)
    sheet_width = CANVAS_SIZE * 3
    for index, image in enumerate(images):
        pixels = list(image.pixels[:])
        column = index % 3
        top_row = index // 3
        destination_row = 1 - top_row
        for y in range(CANVAS_SIZE):
            source_start = y * CANVAS_SIZE * 4
            destination_start = (
                ((destination_row * CANVAS_SIZE + y) * sheet_width +
                 column * CANVAS_SIZE) * 4
            )
            sheet_pixels[destination_start:destination_start + CANVAS_SIZE * 4] = (
                pixels[source_start:source_start + CANVAS_SIZE * 4]
            )
    output.pixels.foreach_set(sheet_pixels)
    output.filepath_raw = str(SHEET_PATH)
    output.file_format = "PNG"
    output.save()


def main() -> None:
    reset_scene()
    PARTS_ROOT.mkdir(parents=True, exist_ok=True)
    FRAMES_ROOT.mkdir(parents=True, exist_ok=True)
    paths = {crop.name: extract_crop(crop) for crop in CROPS}
    build_camera()
    parts = build_rig(paths)
    author_poses(parts)
    configure_render()
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    bpy.ops.render.render(animation=True)
    assemble_sheet()
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    print(f"Saved editable rig: {BLEND_PATH}")
    print(f"Saved sprite sheet: {SHEET_PATH}")


if __name__ == "__main__":
    main()
