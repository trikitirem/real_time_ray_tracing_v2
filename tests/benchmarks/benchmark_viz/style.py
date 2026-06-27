"""Shared matplotlib styling and backend display labels."""

from __future__ import annotations

from pathlib import Path

import matplotlib as mpl
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter

BACKEND_ORDER = ["raster", "rt_full", "rt_shadows"]

BACKEND_LABELS: dict[str, str] = {
    "raster": "Bez ray tracingu",
    "rt_full": "Ray tracing (pełny)",
    "rt_shadows": "Ray tracing (tylko cienie)",
}

BACKEND_COLORS: dict[str, str] = {
    "raster": "#2563eb",
    "rt_full": "#ea580c",
    "rt_shadows": "#16a34a",
}

TABLE_ROW_EVEN = "#f5f5f5"
TABLE_ROW_ODD = "#ffffff"
TABLE_HEADER = "#e0e0e0"

FONT_FAMILY = "DejaVu Sans"
FONT_SIZE = 11
TITLE_SIZE = 13
FIGURE_DPI = 150

# Heatmap: show every Nth stress_count (20 points → 10 columns at stride 2).
HEATMAP_STRESS_STRIDE = 2

VALUE_DECIMALS = 1


def format_fps(value: float) -> str:
    return f"{value:.{VALUE_DECIMALS}f}"


def format_pct(value: float) -> str:
    return f"{value:.{VALUE_DECIMALS}f}%"


def apply_y_axis_one_decimal(ax: plt.Axes) -> None:
    ax.yaxis.set_major_formatter(FormatStrFormatter(f"%.{VALUE_DECIMALS}f"))


def apply_style() -> None:
    mpl.use("Agg")
    mpl.rcParams.update(
        {
            "font.family": FONT_FAMILY,
            "font.size": FONT_SIZE,
            "axes.titlesize": TITLE_SIZE,
            "axes.labelsize": FONT_SIZE,
            "legend.fontsize": FONT_SIZE - 1,
            "xtick.labelsize": FONT_SIZE - 1,
            "ytick.labelsize": FONT_SIZE - 1,
            "figure.dpi": FIGURE_DPI,
            "savefig.dpi": FIGURE_DPI,
            "axes.grid": True,
            "grid.alpha": 0.35,
            "axes.axisbelow": True,
        }
    )


def backend_label(backend_key: str) -> str:
    return BACKEND_LABELS.get(backend_key, backend_key)


def backend_color(backend_key: str) -> str:
    return BACKEND_COLORS.get(backend_key, "#333333")


def save_figure(fig: plt.Figure, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close(fig)
