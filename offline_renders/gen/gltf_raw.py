"""Parses a (small, ASCII, single-external-buffer) glTF file the same way the
engine does: reads raw accessor data straight out of the binary buffer and
NEVER applies any node-hierarchy transform.

Mirrors src/scene/gltf_loader.cpp:
  - load_gltf_models iterates gltf.meshes directly, never touches gltf.nodes
    (gltf_loader.cpp:201-219) -- so any transform baked into a glTF node's own
    "matrix" (Duck.gltf's root node bakes in a 0.01 scale) is ignored by the
    engine and must be ignored here too, or the duck won't match.
  - build_primitive reads POSITION/NORMAL/TEXCOORD_0 accessors as raw floats
    (gltf_loader.cpp:87-99).
  - make_material reads materials[i].pbrMetallicRoughness directly
    (gltf_loader.cpp:33-65).
"""

from __future__ import annotations

import json
import struct
from dataclasses import dataclass
from pathlib import Path

from .mathutil import Vec3

_COMPONENT_TYPE_FORMAT = {
    5121: ("B", 1),  # unsigned byte
    5123: ("H", 2),  # unsigned short
    5125: ("I", 4),  # unsigned int
    5126: ("f", 4),  # float
}

_TYPE_COMPONENT_COUNT = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
}


@dataclass
class RawMaterial:
    base_color: Vec3
    metalness: float
    roughness: float
    texture_uri: str | None  # filename, resolved relative to the .gltf's own directory


@dataclass
class RawMesh:
    positions: list[Vec3]
    normals: list[Vec3]
    uvs: list[tuple[float, float]]
    indices: list[int]  # flattened triangle list, len % 3 == 0
    material: RawMaterial


def _read_accessor(gltf: dict, buffer_bytes: bytes, accessor_index: int) -> list[tuple[float, ...]]:
    accessor = gltf["accessors"][accessor_index]
    buffer_view = gltf["bufferViews"][accessor["bufferView"]]

    component_type = accessor["componentType"]
    fmt_char, component_size = _COMPONENT_TYPE_FORMAT[component_type]
    components_per_elem = _TYPE_COMPONENT_COUNT[accessor["type"]]
    elem_size = component_size * components_per_elem

    base_offset = buffer_view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = buffer_view.get("byteStride", elem_size)
    count = accessor["count"]

    fmt = "<" + fmt_char * components_per_elem
    out: list[tuple[float, ...]] = []
    for i in range(count):
        offset = base_offset + i * stride
        out.append(struct.unpack_from(fmt, buffer_bytes, offset))
    return out


def _read_material(gltf: dict, material_index: int) -> RawMaterial:
    if material_index < 0 or material_index >= len(gltf.get("materials", [])):
        return RawMaterial(base_color=(1.0, 1.0, 1.0), metalness=0.0, roughness=0.5, texture_uri=None)

    gm = gltf["materials"][material_index]
    pbr = gm.get("pbrMetallicRoughness", {})
    base_color_factor = pbr.get("baseColorFactor", [1.0, 1.0, 1.0, 1.0])
    metalness = float(pbr.get("metallicFactor", 1.0))
    roughness = float(pbr.get("roughnessFactor", 1.0))

    texture_uri: str | None = None
    base_color_texture = pbr.get("baseColorTexture")
    if base_color_texture is not None:
        tex_index = base_color_texture["index"]
        texture = gltf["textures"][tex_index]
        image = gltf["images"][texture["source"]]
        uri = image.get("uri")
        if uri:
            texture_uri = uri

    return RawMaterial(
        base_color=(float(base_color_factor[0]), float(base_color_factor[1]), float(base_color_factor[2])),
        metalness=metalness,
        roughness=roughness,
        texture_uri=texture_uri,
    )


def load_duck(gltf_path: Path) -> RawMesh:
    gltf = json.loads(gltf_path.read_text(encoding="utf-8"))

    if len(gltf["buffers"]) != 1:
        raise ValueError("gltf_raw.load_duck only supports a single external buffer")
    buffer_uri = gltf["buffers"][0]["uri"]
    buffer_bytes = (gltf_path.parent / buffer_uri).read_bytes()

    if len(gltf["meshes"]) != 1 or len(gltf["meshes"][0]["primitives"]) != 1:
        raise ValueError("gltf_raw.load_duck only supports a single mesh with a single primitive")
    primitive = gltf["meshes"][0]["primitives"][0]

    positions_raw = _read_accessor(gltf, buffer_bytes, primitive["attributes"]["POSITION"])
    normals_raw = _read_accessor(gltf, buffer_bytes, primitive["attributes"]["NORMAL"])
    uvs_raw = _read_accessor(gltf, buffer_bytes, primitive["attributes"]["TEXCOORD_0"])
    indices_raw = _read_accessor(gltf, buffer_bytes, primitive["indices"])

    positions: list[Vec3] = [(p[0], p[1], p[2]) for p in positions_raw]
    normals: list[Vec3] = [(n[0], n[1], n[2]) for n in normals_raw]
    uvs: list[tuple[float, float]] = [(uv[0], uv[1]) for uv in uvs_raw]
    indices: list[int] = [int(i[0]) for i in indices_raw]

    if len(indices) % 3 != 0:
        raise ValueError(f"index count {len(indices)} is not a multiple of 3")

    material = _read_material(gltf, primitive.get("material", -1))

    return RawMesh(positions=positions, normals=normals, uvs=uvs, indices=indices, material=material)
