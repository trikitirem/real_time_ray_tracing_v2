"""Emits POV-Ray scene.inc, duck_mesh2.inc and one <preset>.pov per camera preset.

Design notes (see plan for full rationale):
  - Floor and mirror planes are built as finite `mesh2` objects (not POV-Ray's
    infinite `plane`, which tiles image_map infinitely by default and doesn't
    match the engine's finite, stretched-once UV mapping -- src/scene/primitives/plane.hpp).
  - `ambient 1.0` is set explicitly on every finish{} and the overall ambient
    level is controlled once via `global_settings { ambient_light }`, so it
    mirrors the engine's single flat kAmbientStrength constant instead of
    compounding with POV-Ray's own default per-finish ambient value.
  - metalness/roughness -> reflection amount reuses the engine's own
    reflectivity = metalness * (1 - roughness) formula; roughness -> POV
    `roughness` and specular strength are approximations (different axes),
    documented as such in the plan -- this is a qualitative visual comparison.
"""

from __future__ import annotations

from pathlib import Path

from .convert import mirror_triangle_indices, povray_camera_vectors, to_povray
from .geometry import plane_world_mesh
from .mathutil import InstanceTransform, Vec3
from .scene_json import CameraPreset, PlanePrimitive

SPECULAR_STRENGTH = 0.7  # kSpecularStrength, shaders/ray_tracing/default.slang:78


def _fmt(x: float) -> str:
    return f"{x:.6f}"


def _fmt_vec3(v: Vec3) -> str:
    return f"<{_fmt(v[0])}, {_fmt(v[1])}, {_fmt(v[2])}>"


def _fmt_vec2(v: tuple[float, float]) -> str:
    return f"<{_fmt(v[0])}, {_fmt(v[1])}>"


def mesh2_block(
    positions: list[Vec3], normals: list[Vec3], uvs: list[tuple[float, float]], indices: list[int]
) -> str:
    """Returns the *body* of a `mesh2 { ... }` block (caller wraps it)."""
    tri_count = len(indices) // 3
    triangles = [tuple(indices[i : i + 3]) for i in range(0, len(indices), 3)]

    vertex_vectors = ",\n    ".join(_fmt_vec3(p) for p in positions)
    normal_vectors = ",\n    ".join(_fmt_vec3(n) for n in normals)
    uv_vectors = ",\n    ".join(_fmt_vec2(uv) for uv in uvs)
    face_indices = ",\n    ".join(f"<{a}, {b}, {c}>" for a, b, c in triangles)
    uv_indices = ",\n    ".join(f"<{a}, {b}, {c}>" for a, b, c in triangles)

    return (
        f"vertex_vectors {{\n    {len(positions)},\n    {vertex_vectors}\n  }}\n"
        f"  normal_vectors {{\n    {len(normals)},\n    {normal_vectors}\n  }}\n"
        f"  uv_vectors {{\n    {len(uvs)},\n    {uv_vectors}\n  }}\n"
        f"  face_indices {{\n    {tri_count},\n    {face_indices}\n  }}\n"
        f"  uv_indices {{\n    {tri_count},\n    {uv_indices}\n  }}"
    )


def _plane_to_povray_mesh(plane: PlanePrimitive) -> tuple[list[Vec3], list[Vec3], list[tuple[float, float]], list[int]]:
    world_positions, world_normals, uvs, indices = plane_world_mesh(plane)
    povray_positions = [to_povray(p) for p in world_positions]
    povray_normals = [to_povray(n) for n in world_normals]
    povray_indices = mirror_triangle_indices(indices)
    return povray_positions, povray_normals, uvs, povray_indices


def _finish_block(metalness: float, roughness: float) -> str:
    reflectivity = metalness * (1.0 - roughness)
    # POV-Ray's `reflection { X }` ADDS X * (traced reflection color) on top of the surface's own
    # ambient+diffuse+specular response -- it does not blend/lerp between them. The engine instead
    # does reflect_color = lerp(lit_color, reflected_color, reflectivity) (shaders/ray_tracing/
    # default.slang:273-276), i.e. a highly reflective surface shows mostly the reflection and very
    # little of its own local shading. To replicate that here (rather than getting a mirror that
    # glows with its own ambient/specular *in addition to* a near-full-strength reflection, which
    # reads as a too-bright surface with a hard-edged seam where the reflective object begins),
    # scale the local shading terms down by (1 - reflectivity) to match the engine's lerp weighting.
    local_shading_weight = 1.0 - reflectivity
    diffuse = max(0.0, 1.0 - metalness) * local_shading_weight
    # specular scales down with roughness, otherwise a "matte" (high-roughness) surface still
    # gets a full-strength highlight.
    specular = SPECULAR_STRENGTH * (1.0 - roughness) * local_shading_weight
    # KNOWN APPROXIMATION -- see docs/analiza_offline_renderery/probe_specular/README.md, where the
    # two POV-Ray highlight models were measured empirically:
    #   `specular` + `roughness r` is Blinn (halfway vector), effective exponent n ~ 1/r
    #   `phong`    + `phong_size n` is classic Phong (reflect vector), n used literally
    # The engine uses classic Phong, pow(dot(R, V), kShininess=24). Since cos(2t)^n ~ cos(t)^(4n)
    # for small angles, matching the engine here would need roughness = 1/(4*24) ~ 0.0104, or --
    # with no conversion at all -- `phong <specular> phong_size 24`. The 0.1 factor below is an
    # eyeballed rescale that lands ~4x too low an exponent, so highlights come out about twice as
    # wide in angle as the engine's. Left as-is deliberately: changing it would invalidate the
    # already-published renders, and it affects only highlight width, not the shadow/geometry
    # findings in the analysis.
    povray_roughness = max(0.0008, roughness * 0.1)
    lines = (
        f"diffuse {_fmt(diffuse)} ambient {_fmt(local_shading_weight)} "
        f"specular {_fmt(specular)} roughness {_fmt(povray_roughness)}"
    )
    if reflectivity > 0.0:
        lines += f" reflection {{ {_fmt(reflectivity)} }}"
    return f"finish {{ {lines} }}"


def _plane_texture_block(plane: PlanePrimitive) -> str:
    if plane.texture is not None:
        filename = Path(plane.texture).name
        image_directive = "jpeg" if filename.lower().endswith((".jpg", ".jpeg")) else "png"
        pigment = f'uv_mapping image_map {{ {image_directive} "assets/{filename}" gamma srgb }}'
    else:
        pigment = f"color rgb {_fmt_vec3(plane.color)}"
    return f"texture {{\n    pigment {{ {pigment} }}\n    {_finish_block(plane.metalness, plane.roughness)}\n  }}"


def write_scene_inc(
    planes: list[PlanePrimitive],
    light_dir_to_light_povray: Vec3,
    duck_metalness: float,
    duck_roughness: float,
    duck_texture_filename: str | None,
    out_path: Path,
) -> None:
    parts: list[str] = []
    parts.append('#version 3.7;')
    parts.append("global_settings { assumed_gamma 1.0 ambient_light rgb <0.12, 0.12, 0.12> }")
    parts.append("background { color rgb <0.53, 0.81, 0.98> }")
    parts.append("")
    parts.append("// Directional sun light")
    parts.append(
        "light_source {\n"
        f"  {_fmt_vec3((light_dir_to_light_povray[0] * 1000.0, light_dir_to_light_povray[1] * 1000.0, light_dir_to_light_povray[2] * 1000.0))}\n"
        "  color rgb <1, 1, 1>\n"
        "  parallel\n"
        "  point_at <0, 0, 0>\n"
        "}"
    )
    parts.append("")

    for i, plane in enumerate(planes):
        positions, normals, uvs, indices = _plane_to_povray_mesh(plane)
        parts.append(f"// Plane primitive #{i}")
        parts.append(
            f"object {{\n  mesh2 {{\n  {mesh2_block(positions, normals, uvs, indices)}\n  }}\n  {_plane_texture_block(plane)}\n}}"
        )
        parts.append("")

    parts.append('#include "duck_mesh2.inc"')
    if duck_texture_filename is not None:
        duck_pigment = f'uv_mapping image_map {{ png "assets/{duck_texture_filename}" gamma srgb }}'
    else:
        duck_pigment = "color rgb <1, 1, 1>"
    parts.append(
        "object {\n"
        "  Duck_Mesh\n"
        f"  texture {{\n    pigment {{ {duck_pigment} }}\n    {_finish_block(duck_metalness, duck_roughness)}\n  }}\n"
        "}"
    )
    parts.append("")

    out_path.write_text("\n".join(parts), encoding="utf-8")


def write_duck_mesh2(
    positions: list[Vec3], normals: list[Vec3], uvs: list[tuple[float, float]], indices: list[int], out_path: Path
) -> None:
    body = mesh2_block(positions, normals, uvs, indices)
    text = f"#declare Duck_Mesh = mesh2 {{\n  {body}\n}}\n"
    out_path.write_text(text, encoding="utf-8")


def write_ini(preset_name: str, pov_relpath: str, width: int, height: int, out_path: Path) -> None:
    """POV-Ray reads an .ini file's settings automatically when invoked as
    `povray <preset>.ini` -- bakes in the render resolution and output path
    (into a renders/ subfolder, POV-Ray does not create it itself -- the
    caller must make sure it exists) so it doesn't need to be passed with
    +W/+H/+O on the command line every time. `pov_relpath` is relative to
    this .ini file's own directory (e.g. "scene/main_shot.pov").

    Library_Path=scene is required: POV-Ray does not automatically search the
    directory of the main source file for #include's when that file itself
    was given as a relative path with a subdirectory component (e.g.
    "scene/main_shot.pov") -- it only searches the current directory and
    Library_Path entries. Without this, "scene.inc" (and everything it in
    turn pulls in: duck_mesh2.inc, assets/*.jpg/png) fails to resolve."""
    text = (
        f"Input_File_Name={pov_relpath}\n"
        f"Output_File_Name=renders/{preset_name}.png\n"
        "Library_Path=scene\n"
        f"Width={width}\n"
        f"Height={height}\n"
        "Output_File_Type=N\n"
        "Antialias=On\n"
        "Antialias_Threshold=0.3\n"
        "Display=off\n"
    )
    out_path.write_text(text, encoding="utf-8")


def write_camera_pov(preset: CameraPreset, fov_y_rad: float, aspect: float, out_path: Path) -> None:
    xf = InstanceTransform(preset.position, preset.euler_degrees, (1.0, 1.0, 1.0))
    forward_engine, up_engine = xf.basis_vectors()

    location = to_povray(preset.position)
    forward_pov = to_povray(forward_engine)
    up_pov = to_povray(up_engine)

    direction, up_scaled, right_scaled = povray_camera_vectors(forward_pov, up_pov, fov_y_rad, aspect)

    text = (
        f'#include "scene.inc"\n\n'
        "camera {\n"
        "  perspective\n"
        f"  location {_fmt_vec3(location)}\n"
        f"  direction {_fmt_vec3(direction)}\n"
        f"  up {_fmt_vec3(up_scaled)}\n"
        f"  right {_fmt_vec3(right_scaled)}\n"
        "}\n"
    )
    out_path.write_text(text, encoding="utf-8")
