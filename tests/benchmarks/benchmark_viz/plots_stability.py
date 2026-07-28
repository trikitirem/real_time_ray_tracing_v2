"""Stability visualizations: p1_low, S1%, heatmap."""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

from benchmark_viz.load_data import available_backends
from benchmark_viz.style import (
    HEATMAP_STRESS_STRIDE,
    VALUE_DECIMALS,
    apply_x_axis_thousands,
    apply_y_axis_one_decimal,
    backend_color,
    backend_label,
    save_figure,
)

RT_BACKENDS = ["rt_full", "rt_shadows"]


def _plot_metric_lines(
    frames: dict[str, pd.DataFrame],
    backend_keys: list[str],
    *,
    value_col: str,
    min_col: str,
    max_col: str,
    title: str,
    ylabel: str,
    log_y: bool,
) -> plt.Figure:
    fig, ax = plt.subplots(figsize=(10.0, 6.0))

    for backend_key in backend_keys:
        df = frames[backend_key]
        color = backend_color(backend_key)
        label = backend_label(backend_key)

        ax.plot(
            df["stress_count"],
            df[value_col],
            marker="o",
            linewidth=2.0,
            markersize=5,
            color=color,
            label=label,
        )

    if log_y:
        ax.set_yscale("log")

    apply_y_axis_one_decimal(ax)
    apply_x_axis_thousands(ax, frames[backend_keys[0]]["stress_count"])
    ax.set_title(title)
    ax.set_xlabel("Liczba obiektów (w tys.)")
    ax.set_ylabel(ylabel)
    ax.legend()
    return fig


def _plot_s1_lines(frames: dict[str, pd.DataFrame], backend_keys: list[str]) -> plt.Figure:
    fig, ax = plt.subplots(figsize=(10.0, 6.0))

    for backend_key in backend_keys:
        df = frames[backend_key]
        ax.plot(
            df["stress_count"],
            df["s1_pct"],
            marker="o",
            linewidth=2.0,
            markersize=5,
            color=backend_color(backend_key),
            label=backend_label(backend_key),
        )

    apply_y_axis_one_decimal(ax)
    apply_x_axis_thousands(ax, frames[backend_keys[0]]["stress_count"])
    ax.set_title("Stabilność klatek (S1%) — wszystkie tryby")
    ax.set_xlabel("Liczba obiektów (w tys.)")
    ax.set_ylabel("S1% (1% low / średnia FPS)")
    ax.legend()
    return fig


def _plot_s1_heatmap(frames: dict[str, pd.DataFrame], backend_keys: list[str]) -> plt.Figure:
    all_counts = sorted({int(v) for df in frames.values() for v in df["stress_count"]})
    stress_counts = all_counts[::HEATMAP_STRESS_STRIDE]
    matrix = []
    row_labels = []

    for backend_key in backend_keys:
        df = frames[backend_key]
        lookup = df.set_index("stress_count")["s1_pct"]
        matrix.append([lookup.get(count, float("nan")) for count in stress_counts])
        row_labels.append(backend_label(backend_key))

    column_labels = [f"{c:,}".replace(",", " ") for c in stress_counts]
    heatmap_df = pd.DataFrame(
        matrix,
        index=row_labels,
        columns=column_labels,
    )

    fmt = f".{VALUE_DECIMALS}f"
    fig, ax = plt.subplots(figsize=(10.0, 3.5 + len(backend_keys)))
    sns.heatmap(
        heatmap_df,
        annot=True,
        fmt=fmt,
        cmap="RdYlGn",
        linewidths=0.5,
        cbar_kws={"label": "S1%", "format": f"%.{VALUE_DECIMALS}f"},
        ax=ax,
    )
    ax.set_title("Stabilność klatek (S1%) — heatmapa")
    ax.set_xlabel("Liczba obiektów")
    ax.set_ylabel("Tryb renderowania")
    return fig


def generate_stability_plots(frames: dict[str, pd.DataFrame], output_dir: Path) -> list[Path]:
    output_paths: list[Path] = []
    charts_dir = output_dir / "charts"
    charts_dir.mkdir(parents=True, exist_ok=True)

    all_backends = available_backends(frames)
    if not all_backends:
        return output_paths

    fig = _plot_metric_lines(
        frames,
        all_backends,
        value_col="p1_low_fps",
        min_col="p1_low_min",
        max_col="p1_low_max",
        title="1% low FPS — wszystkie tryby",
        ylabel="1% low FPS (skala logarytmiczna)",
        log_y=True,
    )
    out_path = charts_dir / "p1_low_all_modes.png"
    save_figure(fig, out_path)
    output_paths.append(out_path)

    rt_keys = [k for k in RT_BACKENDS if k in frames]
    if len(rt_keys) >= 2:
        fig = _plot_metric_lines(
            frames,
            rt_keys,
            value_col="p1_low_fps",
            min_col="p1_low_min",
            max_col="p1_low_max",
            title="1% low FPS — ray tracing",
            ylabel="1% low FPS",
            log_y=False,
        )
        out_path = charts_dir / "p1_low_rt_only.png"
        save_figure(fig, out_path)
        output_paths.append(out_path)

    fig = _plot_s1_lines(frames, all_backends)
    out_path = charts_dir / "s1_pct_lines.png"
    save_figure(fig, out_path)
    output_paths.append(out_path)

    fig = _plot_s1_heatmap(frames, all_backends)
    out_path = charts_dir / "s1_pct_heatmap.png"
    save_figure(fig, out_path)
    output_paths.append(out_path)

    return output_paths
