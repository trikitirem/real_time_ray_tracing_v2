"""Generate summary tables as PNG images."""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

from benchmark_viz.style import (
    TABLE_HEADER,
    TABLE_ROW_EVEN,
    TABLE_ROW_ODD,
    backend_label,
    format_fps,
    format_pct,
    save_figure,
)


def generate_tables(frames: dict[str, pd.DataFrame], output_dir: Path) -> list[Path]:
    output_paths: list[Path] = []
    tables_dir = output_dir / "tables"
    tables_dir.mkdir(parents=True, exist_ok=True)

    for backend_key, df in frames.items():
        if df.empty:
            continue

        label = backend_label(backend_key)
        mean_s1 = df["s1_pct"].mean()

        cell_data = []
        for _, row in df.iterrows():
            cell_data.append(
                [
                    f"{int(row['stress_count']):,}".replace(",", " "),
                    format_fps(row["avg_fps"]),
                    format_fps(row["p1_low_fps"]),
                    format_pct(row["s1_pct"]),
                ]
            )

        cell_data.append(["", "", "Średnia:", format_pct(mean_s1)])

        col_labels = ["Ilość obiektów", "Średnia FPS", "1% LOW", "S1%"]
        n_rows = len(cell_data)
        fig_height = 0.38 * n_rows + 0.35
        fig, ax = plt.subplots(figsize=(9.0, fig_height))
        ax.axis("off")
        ax.set_xlim(0, 1)
        ax.set_ylim(0, 1)

        table_top = 0.93
        table = ax.table(
            cellText=cell_data,
            colLabels=col_labels,
            loc="upper left",
            bbox=[0, 0, 1, table_top],
            cellLoc="center",
        )
        table.auto_set_font_size(False)
        table.set_fontsize(11)
        table.scale(1.0, 1.45)

        ax.text(
            0.0,
            table_top + 0.015,
            f"Dla trybu {label}:",
            transform=ax.transAxes,
            fontsize=13,
            fontweight="bold",
            ha="left",
            va="bottom",
        )

        fig.subplots_adjust(left=0.02, right=0.98, top=0.99, bottom=0.01)

        for col in range(len(col_labels)):
            cell = table[(0, col)]
            cell.set_facecolor(TABLE_HEADER)
            cell.set_text_props(fontweight="bold")

        for row_idx in range(1, n_rows):
            bg = TABLE_ROW_EVEN if row_idx % 2 == 0 else TABLE_ROW_ODD
            for col in range(len(col_labels)):
                cell = table[(row_idx, col)]
                cell.set_facecolor(bg)

        summary_row = n_rows
        for col in range(len(col_labels)):
            cell = table[(summary_row, col)]
            cell.set_facecolor(TABLE_ROW_EVEN)
            if col >= 2:
                cell.set_text_props(fontweight="bold")

        out_path = tables_dir / f"{backend_key}_table.png"
        save_figure(fig, out_path)
        output_paths.append(out_path)

    return output_paths
