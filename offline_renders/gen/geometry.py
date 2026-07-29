"""Builds engine-space (right-handed, Y-up, un-remapped) world geometry for
planes and the duck model, shared by both the Blender and POV-Ray writers so
the two pipelines can never drift apart on the underlying transform math --
only the coordinate remap (gen/convert.py) differs between them.
"""

from __future__ import annotations

from .gltf_raw import RawMesh
from .mathutil import InstanceTransform, Vec3
from .scene_json import ModelRef, PlanePrimitive

# Local-space unit quad, matches src/scene/primitives/plane.hpp:16-45 exactly
# (built at 1x1, scaled by the instance transform afterward).
QUAD_LOCAL_POSITIONS: tuple[Vec3, ...] = (
    (-0.5, 0.0, -0.5),
    (0.5, 0.0, -0.5),
    (0.5, 0.0, 0.5),
    (-0.5, 0.0, 0.5),
)
QUAD_UVS: tuple[tuple[float, float], ...] = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
QUAD_INDICES: tuple[int, ...] = (0, 2, 1, 0, 3, 2)  # src/scene/primitives/plane.hpp:40


def _flip_v(uvs: list[tuple[float, float]]) -> list[tuple[float, float]]:
    """The engine (and glTF/Duck.gltf) use V=0 at the TOP of the image (image
    loaded top-row-first, UV.y used as-authored -- src/scene/gltf_loader.cpp,
    no stbi_set_flip_vertically_on_load call anywhere). Both Blender's UV
    space and POV-Ray's `uv_mapping`/`image_map` use V=0 at the BOTTOM of the
    image (OpenGL-style convention) -- this is the same flip Blender's own
    glTF importer applies on import. Without this, texture features land
    mirrored top-to-bottom (e.g. the duck's eye texture appearing near the
    beak)."""
    return [(u, 1.0 - v) for u, v in uvs]


def plane_world_mesh(
    plane: PlanePrimitive,
) -> tuple[list[Vec3], list[Vec3], list[tuple[float, float]], list[int]]:
    xf = InstanceTransform(plane.position, plane.euler_degrees, plane.scale)
    positions = [xf.apply_point(p) for p in QUAD_LOCAL_POSITIONS]
    normal = xf.apply_normal((0.0, 1.0, 0.0))
    normals = [normal] * 4
    return positions, normals, _flip_v(list(QUAD_UVS)), list(QUAD_INDICES)


def duck_world_mesh(
    raw: RawMesh, model: ModelRef
) -> tuple[list[Vec3], list[Vec3], list[tuple[float, float]], list[int]]:
    xf = InstanceTransform(model.position, model.euler_degrees, model.scale)
    positions = [xf.apply_point(p) for p in raw.positions]
    normals = [xf.apply_normal(n) for n in raw.normals]
    return positions, normals, _flip_v(list(raw.uvs)), list(raw.indices)
