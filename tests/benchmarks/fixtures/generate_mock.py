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


# GPU time as a share of the mean wall-clock frame time, and how much the worst 1% of GPU frames
# stretches. At light load the GPU tail is nearly flat (the overhead lives outside GPU work); under
# heavy load the GPU tail tracks the wall-clock tail.
GPU_SHARE = 0.86
GPU_TAIL_LIGHT = 1.05
GPU_TAIL_HEAVY = 1.38


def _gpu_fields(avg_fps: float, p1_low_fps: float, load_t: float) -> dict:
    avg_gpu_ms = 1000.0 / avg_fps * GPU_SHARE
    tail = _lerp(GPU_TAIL_LIGHT, GPU_TAIL_HEAVY, load_t)
    p1_gpu_ms = avg_gpu_ms * tail
    return {
        "avg_gpu_ms": round(avg_gpu_ms, 4),
        "p1_gpu_ms": round(p1_gpu_ms, 4),
        "avg_gpu_fps": round(1000.0 / avg_gpu_ms, 2),
        "p1_low_gpu_fps": round(1000.0 / p1_gpu_ms, 2),
        "gpu_frame_count": 3600,
    }


def _cpu_stage_fields(avg_fps: float, load_t: float) -> dict:
    wall_ms = 1000.0 / avg_fps
    # Under light load the CPU barely waits on the GPU; under heavy load the fence wait dominates.
    fence = wall_ms * _lerp(0.05, 0.62, load_t)
    remaining = max(wall_ms - fence, 0.0)
    return {
        "avg_fence_wait_ms": round(fence, 4),
        "avg_acquire_ms": round(remaining * 0.30, 4),
        "avg_record_ms": round(remaining * 0.34, 4),
        "avg_submit_ms": round(remaining * 0.11, 4),
        "avg_present_ms": round(remaining * 0.22, 4),
    }


def _make_runs(avg_fps: float, p1_low_fps: float, load_t: float, rng: random.Random):
    runs = []
    for run_idx in range(RUNS_PER_CONFIG):
        avg_noise = rng.uniform(0.97, 1.03)
        p1_noise = rng.uniform(0.95, 1.05)
        run_avg = avg_fps * avg_noise
        run_p1 = p1_low_fps * p1_noise
        run = {
            "run": run_idx + 1,
            "avg_fps": round(run_avg, 2),
            "p1_low_fps": round(run_p1, 2),
            "frame_count": 3600,
        }
        run.update(_gpu_fields(run_avg, run_p1, load_t))
        run.update(_cpu_stage_fields(run_avg, load_t))
        runs.append(run)

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
            load_t = (stress_count - 10000) / (100000 - 10000)
            avg_fps, p1_low = _base_values(stress_count)[backend_key]
            runs, mean_avg, mean_p1 = _make_runs(avg_fps, p1_low, load_t, rng)
            config = {
                "runs": runs,
                "avg_fps": mean_avg,
                "p1_low_fps": mean_p1,
            }
            config.update(_gpu_fields(mean_avg, mean_p1, load_t))
            config.pop("gpu_frame_count", None)
            config.update(_cpu_stage_fields(mean_avg, load_t))
            backend_obj[str(stress_count)] = config
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
