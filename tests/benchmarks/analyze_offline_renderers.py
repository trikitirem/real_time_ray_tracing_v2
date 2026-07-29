#!/usr/bin/env python3
"""Build comparison tables from the engine vs Blender vs POV-Ray screenshots."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_IMAGES = REPO_ROOT / "tests" / "screenshots" / "offline_renderers"
DEFAULT_OUTPUT = REPO_ROOT / "docs" / "analiza_offline_renderery"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Measure shadow response, image agreement and the mirror-seam artefact."
    )
    parser.add_argument(
        "--images",
        type=Path,
        default=DEFAULT_IMAGES,
        help="Directory with <preset>_<renderer>.png screenshots",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Output directory (writes cienie.{csv,md}, zgodnosc.{csv,md}, szew_lustra.{csv,md})",
    )
    return parser.parse_args()


def main() -> int:
    if str(SCRIPT_DIR) not in sys.path:
        sys.path.insert(0, str(SCRIPT_DIR))

    from benchmark_viz.offline_compare import PRESETS, RENDERERS, compute_and_write

    args = parse_args()
    images_dir = args.images.resolve()
    output_dir = args.output.resolve()

    if not images_dir.is_dir():
        print(f"Error: images dir not found: {images_dir}", file=sys.stderr)
        return 1

    missing = [
        f"{preset}_{renderer}.png"
        for preset in PRESETS
        for renderer in RENDERERS
        if not (images_dir / f"{preset}_{renderer}.png").is_file()
    ]
    if missing:
        print(f"Error: missing screenshots in {images_dir}:", file=sys.stderr)
        for name in missing:
            print(f"  {name}", file=sys.stderr)
        return 1

    shadows, agreement, seam = compute_and_write(images_dir, output_dir)

    print(f"Input:  {images_dir} ({len(PRESETS)} presets x {len(RENDERERS)} renderers)")
    print(f"Output: {output_dir}")
    print("        cienie.csv / .md")
    print("        zgodnosc.csv / .md")
    print("        szew_lustra.csv / .md")
    print()
    print("Shadow response (shadow/lit luminance):")
    for preset, group in shadows.groupby("preset", sort=False):
        parts = " ".join(
            f"{row.renderer}={row.shadow_over_lit:.3f}" for row in group.itertuples()
        )
        engine = group[group["renderer"] == "engine"].iloc[0]
        print(f"  {preset:20s} {parts}   (engine/povray in shadow: "
              f"{engine.ratio_vs_povray_shadow:.3f})")
    print()
    print("Mirror seam artefact (px):")
    for row in seam.itertuples():
        print(f"  {row.preset:20s} engine={row.seam_px_engine:4d} "
              f"povray={row.seam_px_povray:4d} blender={row.seam_px_blender:4d}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
