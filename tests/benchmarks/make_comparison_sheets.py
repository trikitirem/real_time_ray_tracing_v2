#!/usr/bin/env python3
"""Stitch the three renders of each camera preset into one side-by-side sheet.

Panel order is Blender | POV-Ray | engine, separated by a white gutter that marks the transition
between renders. By default there is no outer margin and no frame around the panels, so the only
white in the sheet is the gutter itself; `--border` and `--margin` can add them back.

Within a preset the three screenshots have identical dimensions, so nothing is rescaled — the
panels are pasted at their native size and the sheet is sized to fit.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_IMAGES = REPO_ROOT / "tests" / "screenshots" / "offline_renderers"

PRESETS = ["main_shot", "duck_closeup", "shadow_closeup", "reflection_closeup"]

# Left-to-right panel order, as requested — deliberately not alphabetical.
RENDERER_ORDER = ["blender", "povray", "engine"]

# Output lands in the same directory as the input, so the suffix must never collide with a
# renderer name. Input filenames are built only from PRESETS x RENDERER_ORDER and the directory is
# never globbed, so a generated sheet can never be picked up as an input.
OUTPUT_SUFFIX = "_comparison"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build <preset>_comparison.png sheets: Blender | POV-Ray | engine."
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
        default=None,
        help="Output directory (defaults to --images)",
    )
    parser.add_argument(
        "--presets",
        nargs="*",
        default=None,
        help=f"Presets to build (default: all of {', '.join(PRESETS)})",
    )
    parser.add_argument("--gap", type=int, default=16, help="White gutter between panels [px]")
    parser.add_argument(
        "--border",
        type=int,
        default=0,
        help="Dark frame around each panel [px]; 0 disables it, leaving only the white gutters",
    )
    parser.add_argument(
        "--margin",
        type=int,
        default=0,
        help="White outer margin around the whole sheet [px]; 0 makes the panel frames sit flush "
             "with the sheet edge, leaving white only in the gutters between panels",
    )
    parser.add_argument("--border-color", default="#303030", help="Frame colour")
    parser.add_argument("--background", default="white", help="Sheet background colour")
    return parser.parse_args()


def panel_offsets(panel_width: int, margin: int, gap: int, border: int) -> list[int]:
    """Left x of each panel's frame, in RENDERER_ORDER."""
    stride = panel_width + 2 * border + gap
    return [margin + i * stride for i in range(len(RENDERER_ORDER))]


def sheet_size(panel_width: int, panel_height: int, margin: int, gap: int,
               border: int) -> tuple[int, int]:
    count = len(RENDERER_ORDER)
    width = 2 * margin + count * (panel_width + 2 * border) + (count - 1) * gap
    height = 2 * margin + panel_height + 2 * border
    return width, height


def build_sheet(images_dir: Path, preset: str, args: argparse.Namespace):
    from PIL import Image, ImageDraw

    panels: list = [Image.open(images_dir / f"{preset}_{r}.png") for r in RENDERER_ORDER]

    sizes = {p.size for p in panels}
    if len(sizes) != 1:
        detail = ", ".join(f"{r}={p.size}" for r, p in zip(RENDERER_ORDER, panels))
        raise ValueError(f"{preset}: panels differ in size ({detail}); rescaling is not supported")
    panel_width, panel_height = panels[0].size

    sheet = Image.new(
        "RGB", sheet_size(panel_width, panel_height, args.margin, args.gap, args.border),
        args.background,
    )
    draw = ImageDraw.Draw(sheet) if args.border > 0 else None

    for x, panel in zip(panel_offsets(panel_width, args.margin, args.gap, args.border), panels):
        y = args.margin
        if draw is not None:
            # The frame is a filled dark rectangle; pasting the panel inset by `border` leaves it
            # showing as an outline, which avoids drawing four separate lines.
            draw.rectangle(
                [x, y,
                 x + panel_width + 2 * args.border - 1,
                 y + panel_height + 2 * args.border - 1],
                fill=args.border_color,
            )
        position = (x + args.border, y + args.border)
        # Composite rather than convert("RGB"): the screenshots are RGBA, and this stays correct
        # if one of them ever carries real transparency.
        sheet.paste(panel, position, panel if panel.mode == "RGBA" else None)

    for panel in panels:
        panel.close()

    return sheet


def main() -> int:
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        print(
            "Error: Pillow is required. Run with the repo's package dir, e.g.\n"
            "  PYTHONPATH=.packages python3 make_comparison_sheets.py",
            file=sys.stderr,
        )
        return 1

    args = parse_args()
    images_dir = args.images.resolve()
    output_dir = (args.output or args.images).resolve()

    if not images_dir.is_dir():
        print(f"Error: images dir not found: {images_dir}", file=sys.stderr)
        return 1

    presets = PRESETS
    if args.presets is not None:
        unknown = sorted(set(args.presets) - set(PRESETS))
        if unknown:
            print(f"Error: unknown preset(s): {', '.join(unknown)}", file=sys.stderr)
            return 1
        presets = [p for p in PRESETS if p in set(args.presets)]

    missing = [
        f"{preset}_{renderer}.png"
        for preset in presets
        for renderer in RENDERER_ORDER
        if not (images_dir / f"{preset}_{renderer}.png").is_file()
    ]
    if missing:
        print(f"Error: missing screenshots in {images_dir}:", file=sys.stderr)
        for name in missing:
            print(f"  {name}", file=sys.stderr)
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Input:  {images_dir}")
    print(f"Output: {output_dir}")
    print(f"Order:  {' | '.join(RENDERER_ORDER)}")
    print()
    for preset in presets:
        sheet = build_sheet(images_dir, preset, args)
        out_path = output_dir / f"{preset}{OUTPUT_SUFFIX}.png"
        sheet.save(out_path)
        print(f"  {out_path.name:36s} {sheet.width} x {sheet.height}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
