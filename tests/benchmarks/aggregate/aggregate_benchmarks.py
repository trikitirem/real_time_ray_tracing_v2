#!/usr/bin/env python3
"""Aggregate stress-test benchmark JSON files into CSV for Excel charts."""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path

REQUIRED_MODES = ("raster", "rt_full", "rt_shadows_only")
OPTIONAL_MODES = ("rt_full_textured",)
ALL_MODES = REQUIRED_MODES + OPTIONAL_MODES
STRESS_COUNTS = list(range(5000, 100_001, 5000))


def classify_mode(data: dict) -> str:
    backend = data.get("backend")
    if backend == "raster":
        return "raster"
    if backend == "rt":
        reflections = data.get("rt_reflections_enabled", False)
        textured = data.get("stress_use_texture", False)
        if reflections and textured:
            return "rt_full_textured"
        if reflections:
            return "rt_full"
        return "rt_shadows_only"
    raise ValueError(f"Unknown backend: {backend!r}")


def compute_metrics(frames: list[dict]) -> tuple[float, float, float]:
    fps = [frame["fps"] for frame in frames]
    if not fps:
        raise ValueError("No frames in benchmark data")

    n = len(fps)
    avg = sum(fps) / n
    fps_sorted = sorted(fps)

    def low_fps(percentile: float) -> float:
        count = max(1, math.ceil(n * percentile))
        worst = fps_sorted[:count]
        return sum(worst) / len(worst)

    return avg, low_fps(0.01), low_fps(0.05)


def load_benchmark(path: Path) -> tuple[str, int, tuple[float, float, float]]:
    with path.open(encoding="utf-8") as f:
        data = json.load(f)

    mode = classify_mode(data)
    stress_count = data["stress_count"]
    metrics = compute_metrics(data["frames"])
    return mode, stress_count, metrics


def aggregate_all(input_dir: Path) -> dict[str, dict[int, tuple[float, float, float]]]:
    results: dict[str, dict[int, tuple[float, float, float]]] = {
        mode: {} for mode in ALL_MODES
    }
    seen_files: dict[tuple[str, int], Path] = {}

    json_files = sorted(
        p for p in input_dir.glob("*.json") if p.is_file() and p.parent == input_dir
    )

    if not json_files:
        raise FileNotFoundError(f"No JSON benchmark files found in {input_dir}")

    for path in json_files:
        mode, stress_count, metrics = load_benchmark(path)
        key = (mode, stress_count)
        if key in seen_files:
            raise ValueError(
                f"Duplicate benchmark for mode={mode}, stress_count={stress_count}: "
                f"{seen_files[key].name} and {path.name}"
            )
        seen_files[key] = path
        results[mode][stress_count] = metrics

    missing_required: list[str] = []
    for mode in REQUIRED_MODES:
        for stress_count in STRESS_COUNTS:
            if stress_count not in results[mode]:
                missing_required.append(f"{mode} @ {stress_count}")

    if missing_required:
        raise ValueError(
            f"Missing {len(missing_required)} required benchmark(s):\n  "
            + "\n  ".join(missing_required)
        )

    if results["rt_full_textured"]:
        missing_optional: list[str] = []
        for stress_count in STRESS_COUNTS:
            if stress_count not in results["rt_full_textured"]:
                missing_optional.append(f"rt_full_textured @ {stress_count}")
        if missing_optional:
            raise ValueError(
                f"Partial rt_full_textured data ({len(results['rt_full_textured'])}/"
                f"{len(STRESS_COUNTS)} files). Missing:\n  "
                + "\n  ".join(missing_optional)
            )

    return results


def active_modes(
    results: dict[str, dict[int, tuple[float, float, float]]],
) -> list[str]:
    return [mode for mode in ALL_MODES if results[mode]]


def write_csv(
    path: Path,
    results: dict[str, dict[int, tuple[float, float, float]]],
    modes: list[str],
    metric_index: int,
) -> None:
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["stress_count", *modes])
        for stress_count in STRESS_COUNTS:
            row = [stress_count]
            for mode in modes:
                value = results[mode][stress_count][metric_index]
                row.append(f"{value:.3f}")
            writer.writerow(row)


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    default_input = script_dir.parent

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=default_input,
        help="Directory containing StressTest_*.json files (default: tests/benchmarks)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=script_dir,
        help="Directory for output CSV files (default: tests/benchmarks/aggregate)",
    )
    args = parser.parse_args()

    input_dir = args.input_dir.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Reading benchmarks from: {input_dir}")
    results = aggregate_all(input_dir)
    modes = active_modes(results)

    outputs = {
        "avg_fps.csv": 0,
        "fps_1_low.csv": 1,
        "fps_5_low.csv": 2,
    }
    for filename, metric_index in outputs.items():
        out_path = output_dir / filename
        write_csv(out_path, results, modes, metric_index)
        print(f"Wrote {out_path}")

    optional_count = sum(1 for mode in OPTIONAL_MODES if results[mode])
    print(
        f"Done. Aggregated {len(REQUIRED_MODES)} required mode(s)"
        f"{f' + {optional_count} optional mode(s)' if optional_count else ''}"
        f" x {len(STRESS_COUNTS)} stress levels."
        f" CSV columns: {', '.join(modes)}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
