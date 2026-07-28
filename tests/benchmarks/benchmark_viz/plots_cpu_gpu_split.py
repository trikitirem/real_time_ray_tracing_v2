"""Heatmap of S1% before and after correcting for GPU time.

Answers one question: are the 1%-low outlier frames caused by actual GPU work, or by everything
around it? Built from the per-frame CSV traces the diagnostic suite writes. The supporting numbers
live in the tables produced by frame_tables.py.
"""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

from benchmark_viz.load_frames import find_traces, frame_stats, load_frame_trace
from benchmark_viz.style import VALUE_DECIMALS, backend_label, save_figure


def _config_label(info: dict) -> str:
    return f"{backend_label(info['backend'])} — {info['stress_count'] // 1000}k obiektów"


def plot_s1_heatmap_compact(config_stats: list[tuple[dict, dict]], output_dir: Path) -> Path:
    """S1% before and after the correction, as a compact grid.

    Deliberately not an extension of _plot_s1_heatmap in plots_stability.py: that one is laid out as
    backends x object counts, and the diagnostic run only covers two object counts (which
    HEATMAP_STRESS_STRIDE would then thin down to one column). Here the second axis is the metric.
    """
    rows = [_config_label(info) for info, _ in config_stats]
    matrix = [[s["s1_pct"], s["s1_gpu_pct"]] for _, s in config_stats]

    heatmap_df = pd.DataFrame(
        matrix, index=rows, columns=["S1% z pełnego czasu klatki", "S1% z czasu GPU"]
    )

    fmt = f".{VALUE_DECIMALS}f"
    fig, ax = plt.subplots(figsize=(8.5, 1.6 + 0.9 * len(rows)))
    sns.heatmap(
        heatmap_df,
        annot=True,
        fmt=fmt,
        cmap="RdYlGn",
        vmin=0.0,
        vmax=100.0,
        linewidths=0.5,
        cbar_kws={"label": "S1%", "format": f"%.{VALUE_DECIMALS}f"},
        ax=ax,
    )
    ax.set_title("Stabilność klatek (S1%) przed i po korekcie o czas GPU")
    ax.set_ylabel("Konfiguracja")
    ax.tick_params(axis="y", rotation=0)

    path = output_dir / "charts" / "s1_heatmap_wall_vs_gpu.png"
    save_figure(fig, path)
    return path


def generate_cpu_gpu_split_plots(frames_dir: Path, output_dir: Path) -> list[Path]:
    """Charts for the traces in frames_dir. Returns [] when there are none."""
    traces_meta = find_traces(frames_dir)
    if not traces_meta:
        return []

    # Averaged over every run of a configuration; traces are reduced to stats as they are read so
    # the DataFrames are not all held at once.
    per_config: dict[tuple[str, int], list[dict]] = {}
    info_by_config: dict[tuple[str, int], dict] = {}

    for info, path in traces_meta:
        key = (info["backend"], info["stress_count"])
        per_config.setdefault(key, []).append(frame_stats(load_frame_trace(path)))
        info_by_config.setdefault(key, info)

    config_stats = [
        (
            info_by_config[key],
            {
                "s1_pct": float(np.mean([r["s1_pct"] for r in runs])),
                "s1_gpu_pct": float(np.mean([r["s1_gpu_pct"] for r in runs])),
            },
        )
        for key, runs in per_config.items()
    ]

    return [plot_s1_heatmap_compact(config_stats, output_dir)]
