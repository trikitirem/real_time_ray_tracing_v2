"""Coordinate-system boundary layer. All Blender/POV-Ray axis remapping lives
here and nowhere else in the generator.

Engine world: right-handed, Y-up (src/engine/camera.hpp:17).

- Blender: right-handed, Z-up. Same handedness as the engine -> pure axis
  permutation, determinant +1, no winding-order fix needed.
    (x, y, z)_engine -> (x, -z, y)_blender
  (matches Blender's own glTF importer Y-up -> Z-up convention.)

- POV-Ray: left-handed by default (up +Y, same as engine, but the engine is
  right-handed) -> a single-axis mirror is needed, determinant -1, so every
  POV-Ray triangle's winding must be reversed to stay front-facing.
    (x, y, z)_engine -> (-x, y, z)_povray
"""

from __future__ import annotations

from .mathutil import Vec3, cross, normalize, scale_vec


def to_blender(v: Vec3) -> Vec3:
    x, y, z = v
    return (x, -z, y)


def to_povray(v: Vec3) -> Vec3:
    x, y, z = v
    return (-x, y, z)


def mirror_triangle_indices(indices: list[int]) -> list[int]:
    """Swap the last two indices of every triangle to compensate for the
    determinant -1 axis mirror in to_povray (keeps triangles front-facing)."""
    if len(indices) % 3 != 0:
        raise ValueError("indices length must be a multiple of 3")
    out: list[int] = []
    for i in range(0, len(indices), 3):
        i0, i1, i2 = indices[i], indices[i + 1], indices[i + 2]
        out.extend((i0, i2, i1))
    return out


def povray_camera_vectors(
    forward: Vec3, up: Vec3, fov_y_rad: float, aspect: float
) -> tuple[Vec3, Vec3, Vec3]:
    """Builds POV-Ray explicit camera vectors (direction, up, right) already
    scaled so the resulting FOV matches fov_y_rad without using the `angle`
    keyword (its interaction with explicit up/right/direction vectors is
    version-dependent across POV-Ray 3.7/3.8 -- explicit scaling is the
    robust approach). `forward`/`up` must already be in POV-Ray space (i.e.
    already passed through to_povray individually).

    right is re-derived fresh via up x direction (POV-Ray's own default axes
    -- direction=+Z, up=+Y, right=+X -- satisfy right = up x direction under
    ordinary right-handed cross-product math), rather than remapping an
    engine-space right vector, since the axis mirror already changed
    handedness once and must not be compounded a second time.

    POV-Ray's half-FOV is atan(|up| / (2*|direction|)), not atan(|up|/|direction|)
    -- this is why the well-known POV-Ray default camera (up <0,1,0>,
    direction <0,0,1>) has an ~53.13 degree FOV, not 90: atan(1/2)*2 = 53.13.
    So with |direction|=1, |up| must be 2*tan(half_fov), not tan(half_fov).
    """
    import math

    direction = normalize(forward)
    up_n = normalize(up)
    right_n = normalize(cross(up_n, direction))
    # Re-orthogonalize up in case forward/up weren't perfectly perpendicular.
    up_n = normalize(cross(direction, right_n))

    half_fov = fov_y_rad / 2.0
    scale = 2.0 * math.tan(half_fov)
    up_scaled = scale_vec(up_n, scale)
    right_scaled = scale_vec(right_n, scale * aspect)
    return direction, up_scaled, right_scaled
