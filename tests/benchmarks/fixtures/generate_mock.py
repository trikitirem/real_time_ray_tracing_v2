"""Generate mock StressTest summary JSON for visualization testing."""

from __future__ import annotations

import json
import random
from pathlib import Path

STRESS_COUNTS = list(range(10000, 100001, 10000))
RUNS_PER_CONFIG = 10
RNG_SEED = 42


def _lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def _base_values(stress_count: int) -> dict[str, tuple[float, float]]:
    t = (stress_count - 10000) / (100000 - 10000)

    raster_avg = _lerp(4507.08, 815.04, t)
    raster_p1 = _lerp(2238.52, 583.47, t)

    rt_full_avg = _lerp(420.0, 118.0, t)
    rt_full_p1 = _lerp(210.0, 72.0, t)

    rt_shadows_avg = _lerp(680.0, 185.0, t)
    rt_shadows_p1 = _lerp(340.0, 115.0, t)

    return {
        "raster": (raster_avg, raster_p1),
        "rt_full": (rt_full_avg, rt_full_p1),
        "rt_shadows": (rt_shadows_avg, rt_shadows_p1),
    }


def _make_runs(avg_fps: float, p1_low_fps: float, rng: random.Random) -> list[dict]:
    runs = []
    for run_idx in range(RUNS_PER_CONFIG):
        avg_noise = rng.uniform(0.97, 1.03)
        p1_noise = rng.uniform(0.95, 1.05)
        run_avg = avg_fps * avg_noise
        run_p1 = p1_low_fps * p1_noise
        runs.append(
            {
                "run": run_idx + 1,
                "avg_fps": round(run_avg, 2),
                "p1_low_fps": round(run_p1, 2),
                "frame_count": 3600,
            }
        )

    mean_avg = sum(r["avg_fps"] for r in runs) / len(runs)
    mean_p1 = sum(r["p1_low_fps"] for r in runs) / len(runs)
    return runs, round(mean_avg, 2), round(mean_p1, 2)


def build_mock_summary() -> dict:
    rng = random.Random(RNG_SEED)
    summary = {
        "session_started_at": "2026-06-28T00-00-00",
        "gpu_name": "NVIDIA GeForce RTX 4070 (mock)",
        "window_width": 1920,
        "window_height": 1080,
    }

    for backend_key in ("raster", "rt_full", "rt_shadows"):
        backend_obj = {}
        for stress_count in STRESS_COUNTS:
            avg_fps, p1_low = _base_values(stress_count)[backend_key]
            runs, mean_avg, mean_p1 = _make_runs(avg_fps, p1_low, rng)
            backend_obj[str(stress_count)] = {
                "runs": runs,
                "avg_fps": mean_avg,
                "p1_low_fps": mean_p1,
            }
        summary[backend_key] = backend_obj

    return summary


def main() -> None:
    out_path = Path(__file__).resolve().parent / "mock_stress_test_summary.json"
    summary = build_mock_summary()
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
        f.write("\n")
    print(f"Wrote mock summary -> {out_path}")


if __name__ == "__main__":
    main()
