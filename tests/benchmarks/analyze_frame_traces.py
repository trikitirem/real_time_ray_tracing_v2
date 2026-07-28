#!/usr/bin/env python3
"""Build result tables from the per-frame diagnostic traces (GPU time vs CPU overhead)."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_FRAMES_DIR = SCRIPT_DIR / "results" / "type2"
DEFAULT_OUTPUT = SCRIPT_DIR.parent.parent / "docs" / "analiza_gpu_cpu"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compute S1% (wall-clock vs GPU) and the 1%-low frame decomposition."
    )
    parser.add_argument(
        "--frames-dir",
        type=Path,
        default=DEFAULT_FRAMES_DIR,
        help="Directory with frames_*.csv traces from the diagnostic suite",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Output directory (writes s1_wall_vs_gpu.{csv,md} and spike_decomposition.{csv,md})",
    )
    return parser.parse_args()


def main() -> int:
    if str(SCRIPT_DIR) not in sys.path:
        sys.path.insert(0, str(SCRIPT_DIR))

    from benchmark_viz.frame_tables import compute_and_write
    from benchmark_viz.load_frames import find_traces

    args = parse_args()
    frames_dir = args.frames_dir.resolve()
    output_dir = args.output.resolve()

    if not frames_dir.is_dir():
        print(f"Error: frames dir not found: {frames_dir}", file=sys.stderr)
        return 1

    traces = find_traces(frames_dir)
    if not traces:
        print(f"Error: no frames_*.csv traces in {frames_dir}", file=sys.stderr)
        return 1

    s1, spikes = compute_and_write(frames_dir, output_dir)

    print(f"Input:  {frames_dir} ({len(traces)} traces)")
    print(f"Output: {output_dir}")
    print("        s1_wall_vs_gpu.csv / .md")
    print("        spike_decomposition.csv / .md")
    print()
    print("S1% wall-clock vs GPU:")
    for _, row in s1.iterrows():
        print(f"  {row['backend']:>10s} @ {row['stress_count']:>7d}: "
              f"wall {row['s1_wall_pct']:6.2f}%   gpu {row['s1_gpu_pct']:6.2f}%")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
