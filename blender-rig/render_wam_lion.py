from pathlib import Path
import math

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[1]
GLTF = ROOT / "wam-proof" / "out" / "stage1-lion.gltf"
OUTPUT = ROOT / "wam-proof" / "out" / "blender"


def point_camera(camera, target):
    camera.rotation_euler = (Vector(target) - camera.location).to_track_quat("-Z", "Y").to_euler()


def reset_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for block in list(datablocks):
            if block.users == 0:
                datablocks.remove(block)


def tune_materials():
    for material in bpy.data.materials:
        material.use_nodes = True
        principled = material.node_tree.nodes.get("Principled BSDF")
        if not principled:
            continue
        principled.inputs["Roughness"].default_value = 0.62
        principled.inputs["Specular IOR Level"].default_value = 0.30
        if "mane" in material.name.lower():
            principled.inputs["Roughness"].default_value = 0.78


def smooth_meshes():
    for obj in [item for item in bpy.context.scene.objects if item.type == "MESH"]:
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
        subdivision = obj.modifiers.new(name="HD silhouette", type="SUBSURF")
        subdivision.subdivision_type = "CATMULL_CLARK"
        subdivision.levels = 1
        subdivision.render_levels = 2
        armature_index = next((index for index, modifier in enumerate(obj.modifiers) if modifier.type == "ARMATURE"), None)
        if armature_index is not None:
            while list(obj.modifiers).index(subdivision) > armature_index:
                bpy.context.view_layer.objects.active = obj
                bpy.ops.object.modifier_move_up(modifier=subdivision.name)


def scene_bounds():
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    corners = []
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        corners.extend(evaluated.matrix_world @ vertex.co for vertex in mesh.vertices)
        evaluated.to_mesh_clear()
    low = Vector((min(point.x for point in corners), min(point.y for point in corners), min(point.z for point in corners)))
    high = Vector((max(point.x for point in corners), max(point.y for point in corners), max(point.z for point in corners)))
    return corners, low, high, (low + high) * 0.5


def add_lighting(target):
    world = bpy.context.scene.world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.018, 0.024, 0.040, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.20

    key_data = bpy.data.lights.new("Warm key", type="AREA")
    key_data.energy = 900
    key_data.shape = "DISK"
    key_data.size = 4.0
    key = bpy.data.objects.new("Warm key", key_data)
    key.location = (-3.2, 4.0, 4.0)
    bpy.context.collection.objects.link(key)
    point_camera(key, target)

    rim_data = bpy.data.lights.new("Golden rim", type="AREA")
    rim_data.energy = 650
    rim_data.color = (1.0, 0.48, 0.14)
    rim_data.size = 3.0
    rim = bpy.data.objects.new("Golden rim", rim_data)
    rim.location = (2.8, 2.5, -2.0)
    bpy.context.collection.objects.link(rim)
    point_camera(rim, target)

    fill_data = bpy.data.lights.new("Soft fill", type="AREA")
    fill_data.energy = 450
    fill_data.color = (0.45, 0.62, 1.0)
    fill_data.size = 5.0
    fill = bpy.data.objects.new("Soft fill", fill_data)
    fill.location = (-2.5, 1.8, -3.0)
    bpy.context.collection.objects.link(fill)
    point_camera(fill, target)


def add_camera(corners, target):
    camera_data = bpy.data.cameras.new("Arcade side camera")
    camera = bpy.data.objects.new("Arcade side camera", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = target + Vector((-4.0, 0.0, 0.0))
    camera_data.type = "ORTHO"
    camera_data.lens = 70
    point_camera(camera, target)
    bpy.context.view_layer.update()
    camera_inverse = camera.matrix_world.inverted()
    projected = [camera_inverse @ point for point in corners]
    horizontal = max(point.x for point in projected) - min(point.x for point in projected)
    vertical = max(point.y for point in projected) - min(point.y for point in projected)
    aspect = 768 / 512
    camera_data.ortho_scale = max(vertical * 1.22, horizontal / aspect * 1.22)
    bpy.context.scene.camera = camera


def configure_render():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 768
    scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = True
    scene.render.image_settings.color_depth = "8"
    scene.render.image_settings.compression = 20
    scene.render.fps = 60
    scene.view_settings.look = "AgX - Medium High Contrast"


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    reset_scene()
    bpy.ops.import_scene.gltf(filepath=str(GLTF))
    tune_materials()
    smooth_meshes()
    corners, low, high, target = scene_bounds()
    add_lighting(target)
    add_camera(corners, target)
    configure_render()

    scene = bpy.context.scene
    action_ranges = [action.frame_range for action in bpy.data.actions]
    source_start = min((frame_range[0] for frame_range in action_ranges), default=0.0)
    source_end = max((frame_range[1] for frame_range in action_ranges), default=17.0)
    scene.frame_start = math.floor(source_start)
    scene.frame_end = math.ceil(source_end)

    for output_frame in range(12):
        source_frame = source_start + (source_end - source_start) * output_frame / 12
        frame_number = math.floor(source_frame)
        scene.frame_set(frame_number, subframe=source_frame - frame_number)
        scene.render.filepath = str(OUTPUT / f"lion-walk-{output_frame + 1:02d}.png")
        bpy.ops.render.render(write_still=True)

    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / "stage1-lion-wam.blend"))


main()
