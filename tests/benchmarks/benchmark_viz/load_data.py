"""Load StressTest summary JSON into per-backend DataFrames."""

from __future__ import annotations

import json
import random
from pathlib import Path

import pandas as pd

from benchmark_viz.style import BACKEND_ORDER, backend_label


def load_summary(path: Path) -> tuple[dict, dict[str, pd.DataFrame]]:
    with path.open(encoding="utf-8") as f:
        raw = json.load(f)

    meta = {
        "session_started_at": raw.get("session_started_at", ""),
        "gpu_name": raw.get("gpu_name", ""),
        "window_width": raw.get("window_width", 0),
        "window_height": raw.get("window_height", 0),
    }

    frames: dict[str, pd.DataFrame] = {}
    for backend_key in BACKEND_ORDER:
        if backend_key not in raw:
            continue
        frames[backend_key] = _backend_to_dataframe(backend_key, raw[backend_key])

    return meta, frames


# GPU timing and CPU stage breakdown were added after the first result sets were published, so
# every one of these is read with .get() — summaries produced before that must still load.
GPU_COLUMNS = ("avg_gpu_ms", "p1_gpu_ms", "avg_gpu_fps", "p1_low_gpu_fps")
CPU_STAGE_COLUMNS = (
    "avg_fence_wait_ms",
    "avg_acquire_ms",
    "avg_record_ms",
    "avg_submit_ms",
    "avg_present_ms",
)


def _backend_to_dataframe(backend_key: str, backend_obj: dict) -> pd.DataFrame:
    rows = []
    for stress_str, config in backend_obj.items():
        runs = config.get("runs", [])
        avg_values = [r["avg_fps"] for r in runs]
        p1_values = [r["p1_low_fps"] for r in runs]

        avg_fps = float(config["avg_fps"])
        p1_low = float(config["p1_low_fps"])

        avg_gpu_fps = float(config.get("avg_gpu_fps", float("nan")))
        p1_low_gpu_fps = float(config.get("p1_low_gpu_fps", float("nan")))

        row = {
            "stress_count": int(stress_str),
            "avg_fps": avg_fps,
            "p1_low_fps": p1_low,
            "s1_pct": (p1_low / avg_fps * 100.0) if avg_fps > 0 else 0.0,
            "avg_fps_min": min(avg_values) if avg_values else avg_fps,
            "avg_fps_max": max(avg_values) if avg_values else avg_fps,
            "p1_low_min": min(p1_values) if p1_values else p1_low,
            "p1_low_max": max(p1_values) if p1_values else p1_low,
            "avg_fps_std": pd.Series(avg_values).std(ddof=0) if len(avg_values) > 1 else 0.0,
            "p1_low_std": pd.Series(p1_values).std(ddof=0) if len(p1_values) > 1 else 0.0,
            # Same formula and direction as s1_pct, so the two are directly comparable: a much
            # higher s1_gpu_pct means the slow frames carried no extra GPU work.
            "s1_gpu_pct": (
                p1_low_gpu_fps / avg_gpu_fps * 100.0 if avg_gpu_fps > 0 else float("nan")
            ),
            "label": backend_label(backend_key),
        }
        for col in GPU_COLUMNS + CPU_STAGE_COLUMNS:
            row[col] = float(config.get(col, float("nan")))
        rows.append(row)

    return pd.DataFrame(rows).sort_values("stress_count").reset_index(drop=True)


def has_gpu_timing(df: pd.DataFrame) -> bool:
    """True when the summary carried GPU timestamp data for this backend."""
    return "s1_gpu_pct" in df.columns and df["s1_gpu_pct"].notna().any()


def available_backends(frames: dict[str, pd.DataFrame]) -> list[str]:
    return [k for k in BACKEND_ORDER if k in frames]


def load_random_run(path: Path, backend_key: str, stress_count: int, seed: int = 42) -> dict:
    """Pick one run (deterministically, via seed) from runs[] for a given backend/stress_count."""
    with path.open(encoding="utf-8") as f:
        raw = json.load(f)

    runs = raw[backend_key][str(stress_count)]["runs"]
    return random.Random(seed).choice(runs)
