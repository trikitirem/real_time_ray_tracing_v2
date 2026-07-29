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
PRESET_NAMES = ['main_shot', 'duck_closeup', 'shadow_closeup', 'reflection_closeup']

os.makedirs(OUTPUT_DIR, exist_ok=True)
scene = bpy.context.scene
# Force full HD regardless of entry point.
scene.render.resolution_x = 1920
scene.render.resolution_y = 1080
scene.render.resolution_percentage = 100
for name in PRESET_NAMES:
    cam_obj = bpy.data.objects.get(name)
    if cam_obj is None:
        print(f"WARNING: camera object {name!r} not found -- run import_scene.py first")
        continue
    scene.camera = cam_obj
    scene.render.filepath = os.path.join(OUTPUT_DIR, name + ".png")
    bpy.ops.render.render(write_still=True)
