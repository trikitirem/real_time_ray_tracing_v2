"""Parses scenes/graphics_test.json using the same field defaults as the engine's
own loader (src/scene/scene_loader.cpp:23-61), so this generator stays correct if
someone edits the JSON later without touching this file.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

from .mathutil import Vec3


def _read_vec3(values: list[float]) -> Vec3:
    if len(values) < 3:
        raise ValueError(f"expected vec3 array with 3 elements, got {values!r}")
    return (float(values[0]), float(values[1]), float(values[2]))


@dataclass
class CameraPreset:
    name: str
    position: Vec3
    euler_degrees: Vec3


@dataclass
class PlanePrimitive:
    position: Vec3
    euler_degrees: Vec3
    scale: Vec3
    color: Vec3
    metalness: float
    roughness: float
    texture: str | None  # repo-root-relative path, or None


@dataclass
class ModelRef:
    gltf_path: str  # repo-root-relative path
    position: Vec3
    euler_degrees: Vec3
    scale: Vec3


@dataclass
class SceneData:
    name: str
    initial_camera_position: Vec3
    initial_camera_euler_degrees: Vec3
    camera_presets: list[CameraPreset] = field(default_factory=list)
    planes: list[PlanePrimitive] = field(default_factory=list)
    models: list[ModelRef] = field(default_factory=list)


def load_scene(path: Path) -> SceneData:
    raw = json.loads(path.read_text(encoding="utf-8"))

    initial_camera = raw["initial_camera"]

    presets = [
        CameraPreset(
            name=p["name"],
            position=_read_vec3(p["position"]),
            euler_degrees=_read_vec3(p["euler_degrees"]),
        )
        for p in raw.get("camera_presets", [])
    ]

    planes: list[PlanePrimitive] = []
    for prim in raw.get("primitives", []):
        if prim["type"] != "plane":
            raise ValueError(
                f"scene_json.py only handles 'plane' primitives so far, got {prim['type']!r}"
            )
        planes.append(
            PlanePrimitive(
                position=_read_vec3(prim.get("position", [0.0, 0.0, 0.0])),
                euler_degrees=_read_vec3(prim.get("euler_degrees", [0.0, 0.0, 0.0])),
                scale=_read_vec3(prim.get("scale", [1.0, 1.0, 1.0])),
                color=_read_vec3(prim.get("color", [1.0, 1.0, 1.0])),
                metalness=float(prim.get("metalness", 0.0)),
                roughness=float(prim.get("roughness", 0.5)),
                texture=prim.get("texture"),
            )
        )

    models = [
        ModelRef(
            gltf_path=m["gltf_path"],
            position=_read_vec3(m.get("position", [0.0, 0.0, 0.0])),
            euler_degrees=_read_vec3(m.get("euler_degrees", [0.0, 0.0, 0.0])),
            scale=_read_vec3(m.get("scale", [1.0, 1.0, 1.0])),
        )
        for m in raw.get("models", [])
    ]

    return SceneData(
        name=raw["name"],
        initial_camera_position=_read_vec3(initial_camera["position"]),
        initial_camera_euler_degrees=_read_vec3(initial_camera["euler_degrees"]),
        camera_presets=presets,
        planes=planes,
        models=models,
    )
