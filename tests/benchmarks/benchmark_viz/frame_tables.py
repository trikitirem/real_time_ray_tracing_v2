"""Result tables from the per-frame diagnostic traces.

Two tables:
  1. S1% computed from the full frame time vs from GPU time, per configuration.
  2. What actually happened inside the frames that make up the 1% low — the evidence behind the
     first table's numbers.

Both are averaged over every run of a configuration and exported as CSV plus a Markdown mirror,
following the same shape as comparison.py.
"""

from __future__ import annotations

import math
from pathlib import Path

import numpy as np
import pandas as pd

from benchmark_viz.load_frames import find_traces, frame_stats, load_frame_trace
from benchmark_viz.style import VALUE_DECIMALS, backend_label

# Rows of the decomposition table, in the order they are presented.
DECOMPOSITION_ROWS = [
    ("wall_clock_time_ms", "Pełny czas klatki"),
    ("gpu_time_ms", "Czas GPU"),
    ("cpu_fence_wait_ms", "CPU: oczekiwanie na fence"),
    ("cpu_acquire_ms", "CPU: acquire obrazu"),
    ("cpu_record_ms", "CPU: nagrywanie CB"),
    ("cpu_submit_ms", "CPU: submit"),
    ("cpu_present_ms", "CPU: present"),
]

CPU_STAGE_COLUMNS = [key for key, _ in DECOMPOSITION_ROWS if key.startswith("cpu_")]

MS_DECIMALS = 4


def _group_traces(frames_dir: Path) -> dict[tuple[str, int], list[Path]]:
    grouped: dict[tuple[str, int], list[Path]] = {}
    for info, path in find_traces(frames_dir):
        grouped.setdefault((info["backend"], info["stress_count"]), []).append(path)
    return grouped


# --------------------------------------------------------------------------- table 1


def build_s1_table(frames_dir: Path) -> pd.DataFrame:
    rows = []
    for (backend, stress_count), paths in _group_traces(frames_dir).items():
        stats = [frame_stats(load_frame_trace(p)) for p in paths]

        def mean(field: str) -> float:
            return float(np.mean([s[field] for s in stats]))

        rows.append(
            {
                "backend": backend,
                "stress_count": stress_count,
                "runs": len(stats),
                "frames": int(sum(s["frame_count"] for s in stats)),
                "avg_fps": mean("avg_fps"),
                "p1_low_fps": mean("p1_low_fps"),
                "s1_wall_pct": mean("s1_pct"),
                "avg_gpu_ms": mean("avg_gpu_ms"),
                "p1_gpu_ms": mean("p1_gpu_ms"),
                "avg_gpu_fps": mean("avg_gpu_fps"),
                "p1_low_gpu_fps": mean("p1_low_gpu_fps"),
                "s1_gpu_pct": mean("s1_gpu_pct"),
            }
        )

    df = pd.DataFrame(rows)
    return df.sort_values(["backend", "stress_count"]).reset_index(drop=True)


# --------------------------------------------------------------------------- table 2


def _decompose_run(df: pd.DataFrame) -> dict[str, tuple[float, float]]:
    """Mean of each measured quantity in the 1% slowest frames vs the other 99%."""
    n_slow = max(1, math.ceil(len(df) * 0.01))
    order = df["wall_clock_time_ms"].to_numpy().argsort()
    slow_idx = order[-n_slow:]
    typical_idx = order[:-n_slow]

    out = {}
    for column, _ in DECOMPOSITION_ROWS:
        values = df[column].to_numpy()
        out[column] = (
            float(np.nanmean(values[typical_idx])),
            float(np.nanmean(values[slow_idx])),
        )
    return out


def build_spike_table(frames_dir: Path) -> pd.DataFrame:
    rows = []
    for (backend, stress_count), paths in _group_traces(frames_dir).items():
        per_run = [_decompose_run(load_frame_trace(p)) for p in paths]

        averaged = {
            column: (
                float(np.mean([r[column][0] for r in per_run])),
                float(np.mean([r[column][1] for r in per_run])),
            )
            for column, _ in DECOMPOSITION_ROWS
        }

        # Every share is expressed against the same denominator: how much slower the 1%-low frames
        # were overall. That makes the column add up across rows.
        typical_wall, slow_wall = averaged["wall_clock_time_ms"]
        excess = slow_wall - typical_wall

        for column, label in DECOMPOSITION_ROWS:
            typical, slow = averaged[column]
            delta = slow - typical
            rows.append(
                {
                    "backend": backend,
                    "stress_count": stress_count,
                    "metric": label,
                    "typical_ms": typical,
                    "p1_low_ms": slow,
                    "delta_ms": delta,
                    "share_pct": (delta / excess * 100.0) if excess > 0 else float("nan"),
                }
            )

        # Residual: frame time not covered by any timed stage — the main loop's own work outside
        # draw() (input, camera update, benchmark tick).
        known = sum(averaged[c][1] - averaged[c][0] for c in CPU_STAGE_COLUMNS)
        rows.append(
            {
                "backend": backend,
                "stress_count": stress_count,
                "metric": "Niewyjaśnione (poza draw)",
                "typical_ms": float("nan"),
                "p1_low_ms": float("nan"),
                "delta_ms": excess - known,
                "share_pct": ((excess - known) / excess * 100.0) if excess > 0 else float("nan"),
            }
        )

    df = pd.DataFrame(rows)
    order = {label: i for i, (_, label) in enumerate(DECOMPOSITION_ROWS)}
    order["Niewyjaśnione (poza draw)"] = len(order)
    df["_order"] = df["metric"].map(order)
    df = df.sort_values(["backend", "stress_count", "_order"]).drop(columns="_order")
    return df.reset_index(drop=True)


# --------------------------------------------------------------------------- export


def _format_objects(count: int) -> str:
    return f"{count:,}".replace(",", " ")


def _write_csv_and_md(export: pd.DataFrame, output_prefix: Path, title: str,
                      intro: list[str]) -> tuple[Path, Path]:
    output_prefix.parent.mkdir(parents=True, exist_ok=True)
    csv_path = output_prefix.with_suffix(".csv")
    md_path = output_prefix.with_suffix(".md")

    export.to_csv(csv_path, index=False, encoding="utf-8")

    md_lines = [f"# {title}", ""]
    md_lines.extend(intro)
    md_lines.extend(
        [
            "",
            "| " + " | ".join(export.columns) + " |",
            "| " + " | ".join(["---"] * len(export.columns)) + " |",
        ]
    )
    for _, row in export.iterrows():
        md_lines.append("| " + " | ".join(str(v) for v in row) + " |")
    md_lines.append("")
    md_path.write_text("\n".join(md_lines), encoding="utf-8")

    return csv_path, md_path


def write_s1_outputs(df: pd.DataFrame, output_prefix: Path) -> tuple[Path, Path]:
    export = df.copy()
    export["backend"] = export["backend"].map(backend_label)
    export["stress_count"] = export["stress_count"].map(_format_objects)
    for col in ("avg_fps", "p1_low_fps", "s1_wall_pct", "avg_gpu_fps", "p1_low_gpu_fps",
                "s1_gpu_pct"):
        export[col] = export[col].map(lambda v: f"{v:.{VALUE_DECIMALS}f}")
    for col in ("avg_gpu_ms", "p1_gpu_ms"):
        export[col] = export[col].map(lambda v: f"{v:.{MS_DECIMALS}f}")

    export.columns = [
        "Tryb", "Obiekty", "Przebiegi", "Klatki",
        "Średnia FPS", "1% low FPS", "S1% z pełnego czasu klatki [%]",
        "Średni czas GPU [ms]", "1% low czas GPU [ms]",
        "Średnia FPS z GPU", "1% low FPS z GPU", "S1% z czasu GPU [%]",
    ]

    return _write_csv_and_md(
        export,
        output_prefix,
        "Stabilność klatek: pełny czas klatki vs czas GPU",
        [
            "„Pełny czas klatki\" to czas całej iteracji pętli renderowania; „czas GPU\" to część "
            "tego czasu, w której GPU faktycznie wykonywało polecenia.",
            "S1% = (1% low / średnia) × 100. Obie wersje liczone tą samą formułą "
            "(średnia z 1/dt po najwolniejszym 1% klatek), więc są bezpośrednio porównywalne.",
            "Wartości uśrednione po wszystkich przebiegach danej konfiguracji.",
        ],
    )


def write_spike_outputs(df: pd.DataFrame, output_prefix: Path) -> tuple[Path, Path]:
    export = df.copy()
    export["backend"] = export["backend"].map(backend_label)
    export["stress_count"] = export["stress_count"].map(_format_objects)
    for col in ("typical_ms", "p1_low_ms", "delta_ms"):
        export[col] = export[col].map(lambda v: "" if pd.isna(v) else f"{v:.{MS_DECIMALS}f}")
    export["share_pct"] = export["share_pct"].map(
        lambda v: "" if pd.isna(v) else f"{v:.{VALUE_DECIMALS}f}"
    )

    export.columns = [
        "Tryb", "Obiekty", "Wielkość",
        "Typowa klatka [ms]", "Klatka z 1% low [ms]", "Różnica [ms]", "Udział w nadwyżce [%]",
    ]

    return _write_csv_and_md(
        export,
        output_prefix,
        "Dekompozycja klatek tworzących 1% low",
        [
            "Porównanie 1% najwolniejszych klatek (wg pełnego czasu klatki) z pozostałymi 99%.",
            "Udział liczony względem nadwyżki czasu (wiersz „Pełny czas klatki\").",
            "Wartości uśrednione po wszystkich przebiegach danej konfiguracji.",
            "",
            "Do 100% sumują się **etapy CPU + „Niewyjaśnione\"**, bo wykonują się sekwencyjnie "
            "wewnątrz `draw()`. Wiersz **„Czas GPU\" jest odniesieniem, nie składnikiem sumy** — "
            "praca GPU nakłada się czasowo na pracę CPU, więc dodanie jej tworzyłoby całość, "
            "która nie istnieje.",
            "",
            "Kluczowy odczyt: jeśli „Czas GPU\" ma udział bliski zeru, wolne klatki nie zawierają "
            "dodatkowej pracy GPU.",
        ],
    )


def compute_and_write(frames_dir: Path, output_dir: Path) -> tuple[pd.DataFrame, pd.DataFrame]:
    s1 = build_s1_table(frames_dir)
    spikes = build_spike_table(frames_dir)
    write_s1_outputs(s1, output_dir / "s1_wall_vs_gpu")
    write_spike_outputs(spikes, output_dir / "spike_decomposition")
    return s1, spikes
