"""Illustrative per-frame FPS chart explaining the '1% low' metric on one real run."""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D

from benchmark_viz.load_data import load_random_run
from benchmark_viz.style import COLOR_BAD, COLOR_GOOD, apply_y_axis_one_decimal, save_figure

BACKEND_KEY = "raster"
STRESS_COUNT = 10000
SEED = 42

COLOR_AVG = "#2563eb"

# Real runs have 100k+ frames - plotting all of them collapses into an unreadable
# solid block at chart resolution. Illustrate a readable-sized excerpt instead,
# calibrated to the same avg_fps / p1_low_fps as the real chosen run.
SAMPLE_FRAME_COUNT = 600


def _synthesize_frame_fps(
    avg_fps: float, p1_low_fps: float, frame_count: int, seed: int
) -> tuple[np.ndarray, float]:
    """Synthesize a per-frame FPS series whose mean is avg_fps and whose worst 1% mean is p1_low_fps."""
    rng = np.random.default_rng(seed)
    num_low = max(1, round(frame_count * 0.01))
    num_base = frame_count - num_low

    base_mean = (avg_fps * frame_count - p1_low_fps * num_low) / num_base
    base = base_mean + rng.normal(0.0, base_mean * 0.03, num_base)
    base += base_mean - base.mean()

    low = p1_low_fps + rng.normal(0.0, p1_low_fps * 0.05, num_low)
    ceiling = base.min() * 0.98
    low = np.minimum(low, ceiling)
    low += p1_low_fps - low.mean()
    low = np.minimum(low, ceiling)

    values = np.concatenate([base, low])
    order = rng.permutation(frame_count)
    fps = np.empty(frame_count)
    fps[order] = values

    threshold = float(low.max())
    return fps, threshold


def generate_frame_illustration_plot(input_path: Path, output_dir: Path) -> Path:
    run = load_random_run(input_path, BACKEND_KEY, STRESS_COUNT, seed=SEED)
    avg_fps = float(run["avg_fps"])
    p1_low_fps = float(run["p1_low_fps"])
    run_number = int(run["run"])

    frame_count = SAMPLE_FRAME_COUNT
    fps, threshold = _synthesize_frame_fps(avg_fps, p1_low_fps, frame_count, seed=SEED)

    # X axis in elapsed time (ms), derived from each frame's instantaneous duration (1 / fps),
    # so the time axis and the "frames per second" axis stay logically consistent.
    dt = 1.0 / fps
    x = (np.cumsum(dt) - dt[0]) * 1000.0
    fig, ax = plt.subplots(figsize=(10.0, 6.0))

    ymin = fps.min() * 0.95
    ymax = fps.max() * 1.05
    ax.axhspan(ymin, threshold, color=COLOR_BAD, alpha=0.1, zorder=0)

    ax.plot(x, fps, color=COLOR_GOOD, linewidth=1.0, zorder=2)

    low_mask = fps <= threshold
    ax.scatter(x[low_mask], fps[low_mask], color=COLOR_BAD, s=20, zorder=4)

    ax.axhline(threshold, color=COLOR_BAD, linestyle="--", linewidth=1.4, zorder=3)
    ax.axhline(avg_fps, color=COLOR_AVG, linestyle="-.", linewidth=1.8, zorder=3)

    legend_handles = [
        Line2D([0], [0], color=COLOR_GOOD, linewidth=2.0, label="Liczba klatek na sekundę"),
        Line2D(
            [0],
            [0],
            color=COLOR_BAD,
            marker="o",
            linestyle="None",
            markersize=6,
            label="1% najgorszych klatek",
        ),
        Line2D(
            [0], [0], color=COLOR_BAD, linestyle="--", linewidth=1.4, label=f"próg 1% low ({threshold:.1f} FPS)"
        ),
        Line2D([0], [0], color=COLOR_AVG, linestyle="-.", linewidth=1.8, label=f"średnia ({avg_fps:.1f} FPS)"),
    ]

    ax.set_xlim(x[0], x[-1])
    ax.set_ylim(ymin, ymax)
    apply_y_axis_one_decimal(ax)
    ax.set_title(
        f"Ilustracja metryki 1% low FPS — Bez ray tracingu (10 000 obiektów, przebieg #{run_number})"
    )
    ax.set_xlabel("Czas (ms, fragment przebiegu)")
    ax.set_ylabel("Liczba klatek na sekundę (FPS)")
    ax.legend(handles=legend_handles, loc="upper right")

    out_path = output_dir / "charts" / "p1_low_illustration.png"
    save_figure(fig, out_path)
    return out_path
