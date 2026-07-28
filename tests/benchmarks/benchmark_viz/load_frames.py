"""Load per-frame CSV traces written by the diagnostic suite.

Unlike the summary JSON, these hold one row per rendered frame: the wall-clock frame time, the GPU
command buffer execution time, and the cost of each blocking CPU stage.
"""

from __future__ import annotations

import math
import re
from pathlib import Path

import numpy as np
import pandas as pd

# frames_<backend>_<stress_count>_run<k>_<timestamp>.csv
TRACE_PATTERN = re.compile(r"^frames_(?P<backend>.+)_(?P<stress>\d+)_run(?P<run>\d+)_(?P<ts>.+)\.csv$")


def parse_trace_name(path: Path) -> dict | None:
    """Extract backend / stress_count / run from a trace filename, or None if it does not match."""
    m = TRACE_PATTERN.match(path.name)
    if m is None:
        return None
    return {
        "backend": m.group("backend"),
        "stress_count": int(m.group("stress")),
        "run": int(m.group("run")),
        "session": m.group("ts"),
    }


def load_frame_trace(path: Path) -> pd.DataFrame:
    """Read one trace. The '#' lines carry run metadata and are skipped."""
    return pd.read_csv(path, comment="#")


def find_traces(frames_dir: Path) -> list[tuple[dict, Path]]:
    """All parseable traces in a directory, sorted by backend, object count, then run."""
    found = []
    for path in sorted(frames_dir.glob("frames_*.csv")):
        info = parse_trace_name(path)
        if info is not None:
            found.append((info, path))
    found.sort(key=lambda item: (item[0]["backend"], item[0]["stress_count"], item[0]["run"]))
    return found


def _mean_low_fps(seconds: np.ndarray, percentile: float = 0.01) -> float:
    """Mean of 1/dt over the slowest `percentile` of frames.

    Mirrors percentile_low_fps() in src/engine/benchmark.cpp so numbers computed here can be
    checked against the ones the engine wrote into the summary JSON.
    """
    if seconds.size == 0:
        return float("nan")
    count = max(1, math.ceil(seconds.size * percentile))
    slowest = np.sort(seconds)[-count:]
    with np.errstate(divide="ignore"):
        fps = np.where(slowest > 0.0, 1.0 / slowest, 0.0)
    return float(fps.mean())


def _mean_high_ms(values_ms: np.ndarray, percentile: float = 0.01) -> float:
    """Mean of the slowest `percentile` of durations.

    Mirrors percentile_high_ms() in src/engine/benchmark.cpp, so p1_gpu_ms computed here can be
    compared directly against the value the engine wrote into the summary JSON.
    """
    if values_ms.size == 0:
        return float("nan")
    count = max(1, math.ceil(values_ms.size * percentile))
    return float(np.sort(values_ms)[-count:].mean())


def frame_stats(df: pd.DataFrame) -> dict:
    """Wall-clock and GPU averages plus both S1% ratios for a single trace."""
    wall_ms = df["wall_clock_time_ms"].dropna().to_numpy()
    gpu_ms = df["gpu_time_ms"].dropna().to_numpy()
    wall_s = wall_ms / 1000.0
    gpu_s = gpu_ms / 1000.0

    stats = {
        "frame_count": int(wall_s.size),
        "gpu_frame_count": int(gpu_s.size),
        "avg_wall_ms": float(wall_ms.mean()) if wall_ms.size else float("nan"),
        "avg_gpu_ms": float(gpu_ms.mean()) if gpu_ms.size else float("nan"),
        "p1_wall_ms": _mean_high_ms(wall_ms),
        "p1_gpu_ms": _mean_high_ms(gpu_ms),
    }

    with np.errstate(divide="ignore"):
        stats["avg_fps"] = float(np.where(wall_s > 0, 1.0 / wall_s, 0.0).mean()) if wall_s.size else float("nan")
        stats["avg_gpu_fps"] = float(np.where(gpu_s > 0, 1.0 / gpu_s, 0.0).mean()) if gpu_s.size else float("nan")

    stats["p1_low_fps"] = _mean_low_fps(wall_s)
    stats["p1_low_gpu_fps"] = _mean_low_fps(gpu_s)
    stats["s1_pct"] = (
        stats["p1_low_fps"] / stats["avg_fps"] * 100.0 if stats["avg_fps"] > 0 else float("nan")
    )
    stats["s1_gpu_pct"] = (
        stats["p1_low_gpu_fps"] / stats["avg_gpu_fps"] * 100.0
        if stats["avg_gpu_fps"] > 0
        else float("nan")
    )

    return stats
