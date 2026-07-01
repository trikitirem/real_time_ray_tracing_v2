"""FPS ratio comparisons between rendering modes."""

from __future__ import annotations

from pathlib import Path

import pandas as pd

from benchmark_viz.load_data import load_summary
from benchmark_viz.style import VALUE_DECIMALS


def fps_ratio_pct(fps_a: float, fps_b: float) -> float:
    """Ratio of larger FPS to smaller FPS, as percent (e.g. 380% = 3.8x faster)."""
    if fps_a <= 0 or fps_b <= 0:
        return 0.0
    larger = max(fps_a, fps_b)
    smaller = min(fps_a, fps_b)
    return larger / smaller * 100.0


def build_comparison_table(frames: dict[str, pd.DataFrame]) -> pd.DataFrame:
    required = ("raster", "rt_full", "rt_shadows")
    for key in required:
        if key not in frames:
            raise ValueError(f"Missing backend '{key}' in summary JSON")

    raster = frames["raster"].set_index("stress_count")["avg_fps"]
    rt_full = frames["rt_full"].set_index("stress_count")["avg_fps"]
    rt_shadows = frames["rt_shadows"].set_index("stress_count")["avg_fps"]

    stress_counts = sorted(set(raster.index) & set(rt_full.index) & set(rt_shadows.index))
    rows = []
    for count in stress_counts:
        fps_r = float(raster[count])
        fps_full = float(rt_full[count])
        fps_shadows = float(rt_shadows[count])
        rows.append(
            {
                "stress_count": int(count),
                "fps_raster": fps_r,
                "fps_rt_full": fps_full,
                "fps_rt_shadows": fps_shadows,
                "raster_vs_rt_full_pct": fps_ratio_pct(fps_r, fps_full),
                "raster_vs_rt_shadows_pct": fps_ratio_pct(fps_r, fps_shadows),
                "rt_full_vs_rt_shadows_pct": fps_ratio_pct(fps_full, fps_shadows),
            }
        )

    return pd.DataFrame(rows)


def _format_objects(count: int) -> str:
    return f"{count:,}".replace(",", " ")


def write_comparison_outputs(df: pd.DataFrame, output_prefix: Path) -> tuple[Path, Path]:
    output_prefix.parent.mkdir(parents=True, exist_ok=True)
    csv_path = output_prefix.with_suffix(".csv")
    md_path = output_prefix.with_suffix(".md")

    export = df.copy()
    export["stress_count"] = export["stress_count"].map(_format_objects)
    for col in (
        "fps_raster",
        "fps_rt_full",
        "fps_rt_shadows",
        "raster_vs_rt_full_pct",
        "raster_vs_rt_shadows_pct",
        "rt_full_vs_rt_shadows_pct",
    ):
        export[col] = export[col].map(lambda v: f"{v:.{VALUE_DECIMALS}f}")

    export.columns = [
        "Obiekty",
        "FPS bez RT",
        "FPS RT pełny",
        "FPS RT cienie",
        f"Bez RT vs RT pełny [%]",
        f"Bez RT vs RT cienie [%]",
        f"RT pełny vs RT cienie [%]",
    ]
    export.to_csv(csv_path, index=False)

    md_lines = [
        "# Porównanie wydajności (stosunek FPS)",
        "",
        "Stosunek [%] = (większa średnia FPS / mniejsza średnia FPS) × 100.",
        "Np. 380% oznacza, że szybszy tryb ma 3,8× więcej klatek.",
        "",
        "| " + " | ".join(export.columns) + " |",
        "| " + " | ".join(["---"] * len(export.columns)) + " |",
    ]
    for _, row in export.iterrows():
        md_lines.append("| " + " | ".join(str(v) for v in row) + " |")
    md_lines.append("")
    md_path.write_text("\n".join(md_lines), encoding="utf-8")

    return csv_path, md_path


def compute_and_write(input_path: Path, output_prefix: Path) -> pd.DataFrame:
    _, frames = load_summary(input_path)
    df = build_comparison_table(frames)
    write_comparison_outputs(df, output_prefix)
    return df
