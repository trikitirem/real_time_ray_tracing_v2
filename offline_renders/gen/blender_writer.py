"""Emits a standalone bpy script (import_scene.py) that builds the
graphics_test scene, and a lightweight render_all.py convenience script.

Meshes are built directly via bmesh from literal vertex/UV/index arrays
embedded in the generated file -- NOT via Blender's built-in glTF importer,
which would respect Duck.gltf's internal node hierarchy (baking in an extra
0.01 scale) and desync from the engine's raw-accessor interpretation. See
gen/gltf_raw.py and the plan for the full rationale.

Simplification: face normals are recomputed by Blender (bm.normal_update() +
smooth shading) rather than re-importing the glTF's authored per-vertex
normals -- visually indistinguishable for this mesh, and avoids the
version-fragile custom-split-normals API. This is a qualitative visual
comparison, not a pixel-exact one.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .mathutil import Vec3


@dataclass
class MeshData:
    positions: list[Vec3]
    uvs: list[tuple[float, float]]
    indices: list[int]  # flattened triangle list, winding as authored (no mirror fix needed for Blender)


@dataclass
class MaterialParams:
    metallic: float
    roughness: float
    base_color: Vec3
    texture_filename: str | None  # relative to assets/, or None for flat color


@dataclass
class CameraPresetBlender:
    name: str
    position: Vec3
    forward: Vec3
    up: Vec3


def _py_vec3_list(vectors: list[Vec3]) -> str:
    return "[" + ", ".join(f"({v[0]:.6f}, {v[1]:.6f}, {v[2]:.6f})" for v in vectors) + "]"


def _py_vec2_list(vectors: list[tuple[float, float]]) -> str:
    return "[" + ", ".join(f"({v[0]:.6f}, {v[1]:.6f})" for v in vectors) + "]"


def _py_int_list(values: list[int]) -> str:
    return "[" + ", ".join(str(v) for v in values) + "]"


def _mesh_data_literal(var_prefix: str, mesh: MeshData) -> str:
    return (
        f"{var_prefix}_POSITIONS = {_py_vec3_list(mesh.positions)}\n"
        f"{var_prefix}_UVS = {_py_vec2_list(mesh.uvs)}\n"
        f"{var_prefix}_INDICES = {_py_int_list(mesh.indices)}\n"
    )


def _material_call(var_name: str, mat: MaterialParams) -> str:
    tex = f'"{mat.texture_filename}"' if mat.texture_filename else "None"
    return (
        f"{var_name} = make_material("
        f'"{var_name}", {tex}, '
        f"{mat.metallic:.4f}, {mat.roughness:.4f}, "
        f"({mat.base_color[0]:.4f}, {mat.base_color[1]:.4f}, {mat.base_color[2]:.4f}))"
    )


_PRELUDE = '''\
"""Builds the reference comparison scene.

Run headless (also renders all 4 presets to ../renders/blender/):
    blender --background --python import_scene.py -- --render-all

Or paste into Blender's Scripting tab to just build the scene interactively
(camera objects are created but nothing is rendered).
"""

import os
import sys

import bmesh
import bpy
import mathutils

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASSETS_DIR = os.path.join(SCRIPT_DIR, "assets")


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for block in list(bpy.data.meshes):
        if block.users == 0:
            bpy.data.meshes.remove(block)
    for block in list(bpy.data.cameras):
        if block.users == 0:
            bpy.data.cameras.remove(block)
    for block in list(bpy.data.lights):
        if block.users == 0:
            bpy.data.lights.remove(block)


def make_material(name, texture_filename, metallic, roughness, base_color):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if texture_filename:
        image_path = os.path.join(ASSETS_DIR, texture_filename)
        tex_node = mat.node_tree.nodes.new("ShaderNodeTexImage")
        tex_node.image = bpy.data.images.load(image_path)
        mat.node_tree.links.new(tex_node.outputs["Color"], bsdf.inputs["Base Color"])
    else:
        bsdf.inputs["Base Color"].default_value = (*base_color, 1.0)
    return mat


def build_mesh_object(name, positions, uvs, indices, material):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    verts = [bm.verts.new(p) for p in positions]
    bm.verts.ensure_lookup_table()
    uv_layer = bm.loops.layers.uv.new("UVMap")

    made_faces = []
    for i in range(0, len(indices), 3):
        i0, i1, i2 = indices[i], indices[i + 1], indices[i + 2]
        try:
            face = bm.faces.new((verts[i0], verts[i1], verts[i2]))
        except ValueError:
            continue  # duplicate/degenerate face, skip
        face.smooth = True
        made_faces.append((face, (i0, i1, i2)))

    for face, (i0, i1, i2) in made_faces:
        for loop, vi in zip(face.loops, (i0, i1, i2)):
            loop[uv_layer].uv = uvs[vi]

    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def add_camera(name, position, forward, up, fov_y_rad, clip_start, clip_end):
    cam_data = bpy.data.cameras.new(name)
    cam_data.lens_unit = "FOV"
    cam_data.sensor_fit = "VERTICAL"
    cam_data.angle_y = fov_y_rad
    cam_data.clip_start = clip_start
    cam_data.clip_end = clip_end

    cam_obj = bpy.data.objects.new(name, cam_data)
    bpy.context.scene.collection.objects.link(cam_obj)
    cam_obj.location = position

    forward_v = mathutils.Vector(forward).normalized()
    up_v = mathutils.Vector(up).normalized()
    right_v = forward_v.cross(up_v).normalized()
    up_v = right_v.cross(forward_v).normalized()
    backward_v = -forward_v
    # Blender camera local axes: +X right, +Y up, looks down -Z.
    rot_matrix = mathutils.Matrix((tuple(right_v), tuple(up_v), tuple(backward_v))).transposed()
    cam_obj.rotation_mode = "QUATERNION"
    cam_obj.rotation_quaternion = rot_matrix.to_quaternion()
    return cam_obj


def add_sun(emit_direction, energy):
    light_data = bpy.data.lights.new("Sun", type="SUN")
    light_data.energy = energy
    light_data.color = (1.0, 1.0, 1.0)
    light_obj = bpy.data.objects.new("Sun", light_data)
    bpy.context.scene.collection.objects.link(light_obj)
    direction_v = mathutils.Vector(emit_direction).normalized()
    light_obj.rotation_mode = "QUATERNION"
    light_obj.rotation_quaternion = direction_v.to_track_quat("-Z", "Y")
    return light_obj


def setup_world(background_rgb, strength):
    world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
    bpy.context.scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg is not None:
        bg.inputs[0].default_value = (*background_rgb, 1.0)
        bg.inputs[1].default_value = strength

'''


def write_import_scene(
    floor_mesh: MeshData,
    floor_material: MaterialParams,
    mirror_mesh: MeshData,
    mirror_material: MaterialParams,
    duck_mesh: MeshData,
    duck_material: MaterialParams,
    sun_emit_direction: Vec3,
    sun_energy: float,
    world_background_rgb: Vec3,
    world_strength: float,
    fov_y_rad: float,
    z_near: float,
    z_far: float,
    resolution: tuple[int, int],
    presets: list[CameraPresetBlender],
    out_path: Path,
) -> None:
    parts: list[str] = [_PRELUDE]

    parts.append("# --- Mesh data ---\n")
    parts.append(_mesh_data_literal("FLOOR", floor_mesh))
    parts.append(_mesh_data_literal("MIRROR", mirror_mesh))
    parts.append(_mesh_data_literal("DUCK", duck_mesh))

    parts.append("\ndef build_scene():")
    parts.append("    clear_scene()\n")
    parts.append(f"    {_material_call('floor_mat', floor_material)}")
    parts.append(
        '    build_mesh_object("Floor", FLOOR_POSITIONS, FLOOR_UVS, FLOOR_INDICES, floor_mat)\n'
    )
    parts.append(f"    {_material_call('mirror_mat', mirror_material)}")
    parts.append(
        '    build_mesh_object("Mirror", MIRROR_POSITIONS, MIRROR_UVS, MIRROR_INDICES, mirror_mat)\n'
    )
    parts.append(f"    {_material_call('duck_mat', duck_material)}")
    parts.append('    build_mesh_object("Duck", DUCK_POSITIONS, DUCK_UVS, DUCK_INDICES, duck_mat)\n')

    parts.append(
        f"    add_sun({tuple(round(c, 6) for c in sun_emit_direction)}, {sun_energy:.4f})"
    )
    parts.append(
        f"    setup_world({tuple(round(c, 4) for c in world_background_rgb)}, {world_strength:.4f})\n"
    )

    parts.append(f"    bpy.context.scene.render.resolution_x = {resolution[0]}")
    parts.append(f"    bpy.context.scene.render.resolution_y = {resolution[1]}")
    parts.append("    bpy.context.scene.render.resolution_percentage = 100\n")

    parts.append("    cameras = {}")
    for preset in presets:
        parts.append(
            "    cameras[{name!r}] = add_camera({name!r}, {pos}, {fwd}, {up}, {fov:.10f}, {near:.4f}, {far:.4f})".format(
                name=preset.name,
                pos=tuple(round(c, 6) for c in preset.position),
                fwd=tuple(round(c, 6) for c in preset.forward),
                up=tuple(round(c, 6) for c in preset.up),
                fov=fov_y_rad,
                near=z_near,
                far=z_far,
            )
        )
    parts.append("    return cameras\n")

    parts.append(
        '''
def render_all(cameras, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    scene = bpy.context.scene
    for name, cam_obj in cameras.items():
        scene.camera = cam_obj
        scene.render.filepath = os.path.join(output_dir, name + ".png")
        bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    cams = build_scene()
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if "--render-all" in argv:
        render_all(cams, os.path.join(SCRIPT_DIR, "renders"))
'''
    )

    out_path.write_text("\n".join(parts), encoding="utf-8")


def write_render_all(preset_names: list[str], resolution: tuple[int, int], out_path: Path) -> None:
    names_literal = "[" + ", ".join(repr(n) for n in preset_names) + "]"
    text = f'''\
"""Convenience script for when the scene is already built (either the .blend was
saved after running import_scene.py once, or import_scene.py was already run
in this session via Blender's Scripting tab). Renders every camera preset to
./renders/<preset>.png. Change OUTPUT_DIR below if you want the images
somewhere else.
"""

import os

import bpy

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "renders")
PRESET_NAMES = {names_literal}

os.makedirs(OUTPUT_DIR, exist_ok=True)
scene = bpy.context.scene
# Force full HD regardless of entry point.
scene.render.resolution_x = {resolution[0]}
scene.render.resolution_y = {resolution[1]}
scene.render.resolution_percentage = 100
for name in PRESET_NAMES:
    cam_obj = bpy.data.objects.get(name)
    if cam_obj is None:
        print(f"WARNING: camera object {{name!r}} not found -- run import_scene.py first")
        continue
    scene.camera = cam_obj
    scene.render.filepath = os.path.join(OUTPUT_DIR, name + ".png")
    bpy.ops.render.render(write_still=True)
'''
    out_path.write_text(text, encoding="utf-8")
