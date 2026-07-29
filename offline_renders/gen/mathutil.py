"""Pure-Python 3D math matching the engine's conventions exactly.

Rotation composition and instance-transform order mirror:
  src/engine/engine.cpp:26-32, src/scene/transform.hpp:30-37
    orientation = normalize(yaw_rot * pitch_rot * roll_rot)
    where yaw=Y axis, pitch=X axis, roll=Z axis (JSON euler_degrees = [pitch, yaw, roll])
  src/scene/scene_loader.cpp:84-93, src/scene/gltf_loader.cpp:25-31
    instance transform = translate(position) * rotate(euler_degrees) * scale(scale)
"""

from __future__ import annotations

import math

Vec3 = tuple[float, float, float]
Mat3 = tuple[Vec3, Vec3, Vec3]  # rows
Mat4 = tuple[Vec3, Vec3, Vec3, Vec3]  # rows, 4th column omitted except via translation


def vec3(x: float, y: float, z: float) -> Vec3:
    return (x, y, z)


def add(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def scale_vec(a: Vec3, s: float) -> Vec3:
    return (a[0] * s, a[1] * s, a[2] * s)


def dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: Vec3, b: Vec3) -> Vec3:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def length(a: Vec3) -> float:
    return math.sqrt(dot(a, a))


def normalize(a: Vec3) -> Vec3:
    length_ = length(a)
    if length_ == 0.0:
        raise ValueError("cannot normalize a zero-length vector")
    return scale_vec(a, 1.0 / length_)


def mat3_identity() -> Mat3:
    return ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))


def mat3_mul(a: Mat3, b: Mat3) -> Mat3:
    # a, b are rows; standard row-major matrix multiplication a @ b.
    bt = tuple(zip(*b))  # columns of b
    return tuple(tuple(dot(row, col) for col in bt) for row in a)  # type: ignore[return-value]


def mat3_apply(m: Mat3, v: Vec3) -> Vec3:
    return (dot(m[0], v), dot(m[1], v), dot(m[2], v))


def rotate_x(rad: float) -> Mat3:
    c, s = math.cos(rad), math.sin(rad)
    return ((1.0, 0.0, 0.0), (0.0, c, -s), (0.0, s, c))


def rotate_y(rad: float) -> Mat3:
    c, s = math.cos(rad), math.sin(rad)
    return ((c, 0.0, s), (0.0, 1.0, 0.0), (-s, 0.0, c))


def rotate_z(rad: float) -> Mat3:
    c, s = math.cos(rad), math.sin(rad)
    return ((c, -s, 0.0), (s, c, 0.0), (0.0, 0.0, 1.0))


def euler_deg_to_mat3(euler_degrees: Vec3) -> Mat3:
    """euler_degrees = [pitch(X), yaw(Y), roll(Z)] -> R = Ry(yaw) @ Rx(pitch) @ Rz(roll)."""
    pitch_deg, yaw_deg, roll_deg = euler_degrees
    yaw = rotate_y(math.radians(yaw_deg))
    pitch = rotate_x(math.radians(pitch_deg))
    roll = rotate_z(math.radians(roll_deg))
    return mat3_mul(mat3_mul(yaw, pitch), roll)


class InstanceTransform:
    """translate(position) * rotate(euler_degrees) * scale(scale) applied to points/normals."""

    def __init__(self, position: Vec3, euler_degrees: Vec3, scale: Vec3):
        self.position = position
        self.rotation = euler_deg_to_mat3(euler_degrees)
        self.scale = scale

    def apply_point(self, p: Vec3) -> Vec3:
        scaled = (p[0] * self.scale[0], p[1] * self.scale[1], p[2] * self.scale[2])
        rotated = mat3_apply(self.rotation, scaled)
        return add(rotated, self.position)

    def apply_normal(self, n: Vec3) -> Vec3:
        # Engine applies the model matrix's upper-left 3x3 directly to normals with
        # no inverse-transpose (shaders/ray_tracing/default.slang:83) -- replicate as-is.
        scaled = (n[0] * self.scale[0], n[1] * self.scale[1], n[2] * self.scale[2])
        rotated = mat3_apply(self.rotation, scaled)
        return normalize(rotated)

    def basis_vectors(self) -> tuple[Vec3, Vec3]:
        """Returns (forward, up) for a camera-like transform: R @ (0,0,-1), R @ (0,1,0)."""
        forward = mat3_apply(self.rotation, (0.0, 0.0, -1.0))
        up = mat3_apply(self.rotation, (0.0, 1.0, 0.0))
        return forward, up
