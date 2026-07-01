#!/usr/bin/env python3
"""Compute FPS ratio comparisons between rendering modes."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_INPUT = SCRIPT_DIR / "results" / "StressTest_summary.json"
DEFAULT_OUTPUT = SCRIPT_DIR / "results" / "fps_comparison"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compute FPS ratio table (larger/smaller × 100) per stress count."
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=DEFAULT_INPUT,
        help="Path to StressTest summary JSON",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Output path prefix (writes .csv and .md)",
    )
    return parser.parse_args()


def main() -> int:
    if str(SCRIPT_DIR) not in sys.path:
        sys.path.insert(0, str(SCRIPT_DIR))

    from benchmark_viz.comparison import compute_and_write

    args = parse_args()
    input_path = args.input.resolve()
    output_prefix = args.output.resolve()

    if not input_path.is_file():
        print(f"Error: input file not found: {input_path}", file=sys.stderr)
        return 1

    df = compute_and_write(input_path, output_prefix)
    csv_path = output_prefix.with_suffix(".csv")
    md_path = output_prefix.with_suffix(".md")

    print(f"Input:  {input_path}")
    print(f"Rows:   {len(df)} stress levels")
    print(f"Output: {csv_path}")
    print(f"        {md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
