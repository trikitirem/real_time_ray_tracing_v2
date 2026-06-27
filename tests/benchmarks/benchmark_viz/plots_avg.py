"""Average FPS line charts."""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

from benchmark_viz.load_data import available_backends
from benchmark_viz.style import apply_y_axis_one_decimal, backend_color, backend_label, save_figure

RT_BACKENDS = ["rt_full", "rt_shadows"]


def _plot_fps_lines(
    frames: dict[str, pd.DataFrame],
    backend_keys: list[str],
    *,
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
            df["avg_fps"],
            marker="o",
            linewidth=2.0,
            markersize=5,
            color=color,
            label=label,
        )
        ax.fill_between(
            df["stress_count"],
            df["avg_fps_min"],
            df["avg_fps_max"],
            color=color,
            alpha=0.15,
        )

    if log_y:
        ax.set_yscale("log")

    apply_y_axis_one_decimal(ax)
    ax.set_title(title)
    ax.set_xlabel("Liczba obiektów")
    ax.set_ylabel(ylabel)
    ax.legend()
    return fig


def generate_avg_plots(frames: dict[str, pd.DataFrame], output_dir: Path) -> list[Path]:
    output_paths: list[Path] = []
    charts_dir = output_dir / "charts"
    charts_dir.mkdir(parents=True, exist_ok=True)

    all_backends = available_backends(frames)
    if not all_backends:
        return output_paths

    specs = [
        (
            "avg_fps_all_modes.png",
            all_backends,
            "Średnia FPS — wszystkie tryby",
            "Średnia FPS",
            False,
        ),
        (
            "avg_fps_all_modes_log.png",
            all_backends,
            "Średnia FPS — wszystkie tryby (oś Y logarytmiczna)",
            "Średnia FPS (skala logarytmiczna)",
            True,
        ),
    ]

    rt_keys = [k for k in RT_BACKENDS if k in frames]
    if len(rt_keys) >= 2:
        specs.append(
            (
                "avg_fps_rt_only.png",
                rt_keys,
                "Średnia FPS — ray tracing",
                "Średnia FPS",
                False,
            )
        )

    for filename, keys, title, ylabel, log_y in specs:
        fig = _plot_fps_lines(frames, keys, title=title, ylabel=ylabel, log_y=log_y)
        out_path = charts_dir / filename
        save_figure(fig, out_path)
        output_paths.append(out_path)

    return output_paths
