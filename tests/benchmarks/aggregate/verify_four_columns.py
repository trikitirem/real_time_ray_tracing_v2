#!/usr/bin/env python3
"""Verify aggregation produces 4 CSV columns when rt_full_textured data is present."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def make_frames(count: int = 100, base_fps: float = 50.0) -> list[dict]:
    return [{"dt_s": 1.0 / base_fps, "fps": base_fps} for _ in range(count)]


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    src_dir = script_dir.parent
    aggregate_script = script_dir / "aggregate_benchmarks.py"

    with tempfile.TemporaryDirectory(prefix="bench_agg_") as tmp:
        tmp_dir = Path(tmp)
        for path in src_dir.glob("*.json"):
            shutil.copy2(path, tmp_dir / path.name)

        for stress in range(5000, 100_001, 5000):
            data = {
                "backend": "rt",
                "rt_reflections_enabled": True,
                "stress_use_texture": True,
                "stress_count": stress,
                "frames": make_frames(base_fps=40.0 + stress / 5000),
            }
            out_path = tmp_dir / f"StressTest_rt_textured_{stress}.json"
            out_path.write_text(json.dumps(data), encoding="utf-8")

        result = subprocess.run(
            [sys.executable, str(aggregate_script), "--input-dir", str(tmp_dir)],
            capture_output=True,
            text=True,
            check=False,
        )
        print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        if result.returncode != 0:
            return result.returncode

        csv_path = script_dir / "avg_fps.csv"
        header = csv_path.read_text(encoding="utf-8").splitlines()[0]
        columns = header.split(",")
        if columns != [
            "stress_count",
            "raster",
            "rt_full",
            "rt_shadows_only",
            "rt_full_textured",
        ]:
            print(f"Unexpected CSV header: {columns}", file=sys.stderr)
            return 1

    # Restore 3-column CSV from real benchmarks only.
    subprocess.run(
        [sys.executable, str(aggregate_script)],
        check=True,
    )
    print("Verified 4-column aggregation path.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
