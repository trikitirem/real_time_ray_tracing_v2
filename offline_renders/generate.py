#!/usr/bin/env python3
"""Generates Blender (bpy) and POV-Ray scene files from scenes/graphics_test.json,
so the real-time engine's output can be visually compared against two offline
renderers using the exact same camera angles/lighting/materials.

Usage (from repo root, via WSL or any python3 with no extra dependencies):
    python3 offline_renders/generate.py
    python3 offline_renders/generate.py --presets main_shot duck_closeup

See the plan this was generated from for the full technical rationale:
coordinate conventions, the Duck.gltf raw-accessor requirement, camera FOV
math, and material/light mapping approximations.
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
OUTPUT_ROOT = REPO_ROOT / "offline_renders" / "graphics_test"

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gen import blender_writer, convert, geometry, gltf_raw, mathutil, povray_writer, scene_json  # noqa: E402

# Ground-truth engine constants -- see plan / src/engine/camera.hpp:23-25, src/engine/engine.cpp:24-25.
FOV_Y_RAD = 1.0471975511965977461542144610932  # ~60 degrees vertical
Z_NEAR = 0.1
Z_FAR = 300.0
RESOLUTION = (1920, 1080)
ASPECT = RESOLUTION[0] / RESOLUTION[1]

# shaders/ray_tracing/default.slang:74 -- vector FROM the surface TOWARD the light.
LIGHT_DIR_TO_LIGHT = mathutil.normalize((0.0, 1.0, 1.0))
SUN_ENERGY_BLENDER = 3.0  # no principled equivalent to the engine's ad hoc constants -- tune by eye
WORLD_BACKGROUND_RGB = (0.53, 0.81, 0.98)  # src/renderer/raster/raster_frame_recorder.cpp:93
WORLD_STRENGTH_BLENDER = 0.4  # approximates kAmbientStrength=0.12 -- tune by eye


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--scene",
        type=Path,
        default=REPO_ROOT / "scenes" / "graphics_test.json",
        help="Path to the scene JSON (default: scenes/graphics_test.json)",
    )
    parser.add_argument(
        "--presets",
        nargs="*",
        default=None,
        help="Camera preset names to generate (default: all presets in the scene JSON)",
    )
    return parser.parse_args()


def select_single(items: list, description: str):
    if len(items) != 1:
        raise ValueError(f"expected exactly one {description}, found {len(items)}")
    return items[0]


def main() -> None:
    args = parse_args()

    scene = scene_json.load_scene(args.scene)

    textured_planes = [p for p in scene.planes if p.texture]
    flat_planes = [p for p in scene.planes if not p.texture]
    floor = select_single(textured_planes, "textured plane (the floor)")
    mirror = select_single(flat_planes, "untextured plane (the mirror)")
    model = select_single(scene.models, "model (the duck)")

    presets = scene.camera_presets
    if args.presets is not None:
        wanted = set(args.presets)
        presets = [p for p in presets if p.name in wanted]
        missing = wanted - {p.name for p in presets}
        if missing:
            raise ValueError(f"unknown camera preset name(s): {sorted(missing)}")

    duck_gltf_path = REPO_ROOT / model.gltf_path
    raw_duck = gltf_raw.load_duck(duck_gltf_path)

    floor_world = geometry.plane_world_mesh(floor)
    mirror_world = geometry.plane_world_mesh(mirror)
    duck_world = geometry.duck_world_mesh(raw_duck, model)

    blender_dir = OUTPUT_ROOT / "blender"
    povray_dir = OUTPUT_ROOT / "povray"
    povray_scene_dir = povray_dir / "scene"
    blender_assets_dir = blender_dir / "assets"
    povray_assets_dir = povray_scene_dir / "assets"
    povray_renders_dir = povray_dir / "renders"
    for d in (blender_dir, povray_dir, povray_scene_dir, blender_assets_dir, povray_assets_dir, povray_renders_dir):
        d.mkdir(parents=True, exist_ok=True)

    floor_texture_src = REPO_ROOT / floor.texture
    duck_texture_src = duck_gltf_path.parent / raw_duck.material.texture_uri if raw_duck.material.texture_uri else None
    for src in (floor_texture_src, duck_texture_src):
        if src is None:
            continue
        shutil.copy2(src, blender_assets_dir / src.name)
        shutil.copy2(src, povray_assets_dir / src.name)

    # --- Blender ---
    def to_blender_mesh(world_positions, world_normals, uvs, indices) -> blender_writer.MeshData:
        del world_normals  # Blender recomputes smooth normals -- see gen/blender_writer.py docstring
        return blender_writer.MeshData(
            positions=[convert.to_blender(p) for p in world_positions], uvs=uvs, indices=indices
        )

    floor_mesh_blender = to_blender_mesh(*floor_world)
    mirror_mesh_blender = to_blender_mesh(*mirror_world)
    duck_mesh_blender = to_blender_mesh(*duck_world)

    floor_material_blender = blender_writer.MaterialParams(
        metallic=floor.metalness, roughness=floor.roughness, base_color=floor.color,
        texture_filename=Path(floor.texture).name,
    )
    mirror_material_blender = blender_writer.MaterialParams(
        metallic=mirror.metalness, roughness=mirror.roughness, base_color=mirror.color,
        texture_filename=None,
    )
    duck_material_blender = blender_writer.MaterialParams(
        metallic=raw_duck.material.metalness, roughness=raw_duck.material.roughness,
        base_color=raw_duck.material.base_color, texture_filename=raw_duck.material.texture_uri,
    )

    sun_emit_engine = mathutil.scale_vec(LIGHT_DIR_TO_LIGHT, -1.0)
    sun_emit_blender = convert.to_blender(sun_emit_engine)

    camera_presets_blender = []
    for preset in presets:
        xf = mathutil.InstanceTransform(preset.position, preset.euler_degrees, (1.0, 1.0, 1.0))
        forward_engine, up_engine = xf.basis_vectors()
        camera_presets_blender.append(
            blender_writer.CameraPresetBlender(
                name=preset.name,
                position=convert.to_blender(preset.position),
                forward=convert.to_blender(forward_engine),
                up=convert.to_blender(up_engine),
            )
        )

    blender_writer.write_import_scene(
        floor_mesh=floor_mesh_blender,
        floor_material=floor_material_blender,
        mirror_mesh=mirror_mesh_blender,
        mirror_material=mirror_material_blender,
        duck_mesh=duck_mesh_blender,
        duck_material=duck_material_blender,
        sun_emit_direction=sun_emit_blender,
        sun_energy=SUN_ENERGY_BLENDER,
        world_background_rgb=WORLD_BACKGROUND_RGB,
        world_strength=WORLD_STRENGTH_BLENDER,
        fov_y_rad=FOV_Y_RAD,
        z_near=Z_NEAR,
        z_far=Z_FAR,
        resolution=RESOLUTION,
        presets=camera_presets_blender,
        out_path=blender_dir / "import_scene.py",
    )
    blender_writer.write_render_all([p.name for p in presets], RESOLUTION, blender_dir / "render_all.py")

    # --- POV-Ray ---
    duck_positions_pov = [convert.to_povray(p) for p in duck_world[0]]
    duck_normals_pov = [convert.to_povray(n) for n in duck_world[1]]
    duck_indices_pov = convert.mirror_triangle_indices(duck_world[3])
    povray_writer.write_duck_mesh2(
        duck_positions_pov, duck_normals_pov, duck_world[2], duck_indices_pov,
        povray_scene_dir / "duck_mesh2.inc",
    )

    light_dir_to_light_povray = convert.to_povray(LIGHT_DIR_TO_LIGHT)
    povray_writer.write_scene_inc(
        planes=[floor, mirror],
        light_dir_to_light_povray=light_dir_to_light_povray,
        duck_metalness=raw_duck.material.metalness,
        duck_roughness=raw_duck.material.roughness,
        duck_texture_filename=raw_duck.material.texture_uri,
        out_path=povray_scene_dir / "scene.inc",
    )

    for preset in presets:
        povray_writer.write_camera_pov(preset, FOV_Y_RAD, ASPECT, povray_scene_dir / f"{preset.name}.pov")
        povray_writer.write_ini(
            preset.name, f"scene/{preset.name}.pov", RESOLUTION[0], RESOLUTION[1],
            povray_dir / f"{preset.name}.ini",
        )

    print(f"Generated {len(presets)} preset(s): {', '.join(p.name for p in presets)}")
    print(f"Blender: {blender_dir}")
    print(f"POV-Ray: {povray_dir}")


if __name__ == "__main__":
    main()
