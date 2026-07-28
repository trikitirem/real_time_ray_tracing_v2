#!/usr/bin/env python3
"""Generate benchmark visualizations from StressTest summary JSON."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_INPUT = SCRIPT_DIR / "fixtures" / "mock_stress_test_summary.json"
DEFAULT_OUTPUT = SCRIPT_DIR / "output"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate benchmark tables and charts from StressTest summary JSON."
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=DEFAULT_INPUT,
        help=f"Path to summary JSON (default: {DEFAULT_INPUT.name})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Output directory (default: {DEFAULT_OUTPUT.name}/)",
    )
    return parser.parse_args()


def main() -> int:
    if str(SCRIPT_DIR) not in sys.path:
        sys.path.insert(0, str(SCRIPT_DIR))

    from benchmark_viz.load_data import load_summary
    from benchmark_viz.plots_avg import generate_avg_plots
    from benchmark_viz.plots_frame_illustration import generate_frame_illustration_plot
    from benchmark_viz.plots_stability import generate_stability_plots
    from benchmark_viz.plots_tables import generate_tables
    from benchmark_viz.style import apply_style

    args = parse_args()
    input_path = args.input.resolve()
    output_dir = args.output.resolve()

    if not input_path.is_file():
        print(f"Error: input file not found: {input_path}", file=sys.stderr)
        return 1

    apply_style()
    meta, frames = load_summary(input_path)

    if not frames:
        print("Error: no backend data found in summary JSON.", file=sys.stderr)
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

    generated: list[Path] = []
    generated.extend(generate_tables(frames, output_dir))
    generated.extend(generate_avg_plots(frames, output_dir))
    generated.extend(generate_stability_plots(frames, output_dir))
    generated.append(generate_frame_illustration_plot(input_path, output_dir))

    print(f"Input:  {input_path}")
    print(f"GPU:    {meta.get('gpu_name', 'unknown')}")
    print(f"Output: {output_dir}")
    print(f"Generated {len(generated)} files:")
    for path in generated:
        rel = path.relative_to(output_dir)
        print(f"  - {rel}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
