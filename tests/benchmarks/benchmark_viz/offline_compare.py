"""Quantitative comparison of the engine's renders against the offline renderers.

Input is the set of cropped screenshots in tests/screenshots/offline_renderers/, three per
camera preset (engine / POV-Ray / Blender). Within one preset the three files have identical
dimensions, so they are pixel-aligned and can be compared directly; across presets the crops
differ, so every comparison here stays inside a single preset.

Three tables:
  1. Shadow response — the luminance of lit vs shadowed floor per renderer. This is where the
     engine and POV-Ray disagree, and the table checks the measurement against the value
     predicted from the engine's own shader constants.
  2. Agreement — whole-frame RMSE/PSNR between each pair of renderers, plus the background
     colour, which isolates how much of the engine-vs-Blender gap is just exposure.
  3. Mirror seam — how strongly each renderer shows the sky-coloured sliver where the mirror's
     base meets the floor.

All photometry is done in linear light: the PNGs are sRGB-encoded, so taking ratios of the
stored values directly would be meaningless.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
import pandas as pd
from PIL import Image

PRESETS = ["main_shot", "duck_closeup", "shadow_closeup", "reflection_closeup"]
RENDERERS = ["engine", "povray", "blender"]

RENDERER_LABELS = {
    "engine": "Silnik (RT)",
    "povray": "POV-Ray",
    "blender": "Blender (Cycles)",
}

# The seam detector and the background probe both read a patch from the top-left corner and
# assume it is background. In shadow_closeup the camera is pointed down at the floor and no
# background is in frame, so both measurements are skipped for that preset.
PRESETS_WITHOUT_BACKGROUND = {"shadow_closeup"}

# --------------------------------------------------------------------------- predicted values
# Derived from the engine's shader constants in shaders/ray_tracing/default.slang:
#   kAmbientStrength = 0.12, kShadowStrength = 0.2
# For the floor: N = (0,1,0), L = normalize(0,1,1), so N·L = 1/sqrt(2).
N_DOT_L = 1.0 / np.sqrt(2.0)
AMBIENT = 0.12
SHADOW_STRENGTH = 0.2

PRED_ENGINE_LIT = AMBIENT + N_DOT_L
PRED_ENGINE_SHADOW = AMBIENT + SHADOW_STRENGTH * N_DOT_L
PRED_POVRAY_SHADOW = AMBIENT  # POV-Ray occludes the direct light fully; only ambient remains
PRED_ENGINE_CONTRAST = PRED_ENGINE_SHADOW / PRED_ENGINE_LIT
PRED_POVRAY_CONTRAST = PRED_POVRAY_SHADOW / PRED_ENGINE_LIT
PRED_SHADOW_RATIO = PRED_ENGINE_SHADOW / PRED_POVRAY_SHADOW

# Pixels whose engine/POV-Ray luminance ratio exceeds this are taken to be in shadow. The two
# renderers agree to within a few percent in lit areas and differ by ~2.2x in shadow, so the
# distribution is strongly bimodal and the exact threshold does not matter.
SHADOW_RATIO_THRESHOLD = 1.5

BLACK_FLOOR = 0.002  # ignore near-black pixels, where a ratio is numerically meaningless

DECIMALS = 3
MD_DECIMALS = 4


# --------------------------------------------------------------------------- image loading


def load_srgb8(images_dir: Path, preset: str, renderer: str) -> np.ndarray:
    """Screenshot as float64 RGB in 0..255 (sRGB-encoded, as stored)."""
    path = images_dir / f"{preset}_{renderer}.png"
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)


def srgb_to_linear(srgb8: np.ndarray) -> np.ndarray:
    a = srgb8 / 255.0
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)


def luminance(linear: np.ndarray) -> np.ndarray:
    """Rec. 709 relative luminance."""
    return 0.2126 * linear[..., 0] + 0.7152 * linear[..., 1] + 0.0722 * linear[..., 2]


def background_colour(srgb8: np.ndarray, patch: int = 40) -> np.ndarray:
    """Mean sRGB of the top-left corner, used as this image's background reference."""
    return srgb8[:patch, :patch].reshape(-1, 3).mean(axis=0)


# --------------------------------------------------------------------------- masks


def floor_mask(engine_srgb8: np.ndarray, lum_engine: np.ndarray,
               lum_povray: np.ndarray) -> np.ndarray:
    """Floor pixels: everything that is neither the yellow duck nor the sky.

    Heuristic thresholds on the raw channels, not geometry — good enough to separate three very
    differently coloured regions, and applied identically to every renderer so any bias is
    common to all of them.
    """
    linear = srgb_to_linear(engine_srgb8)
    r, g, b = linear[..., 0], linear[..., 1], linear[..., 2]
    duck = (r > b * 1.8) & (g > b * 1.8)
    sky = (b > r * 1.05) & (lum_engine > 0.35)
    return ~duck & ~sky & (lum_engine > BLACK_FLOOR) & (lum_povray > BLACK_FLOOR)


def seam_pixels(srgb8: np.ndarray, gap: int = 15, tolerance: float = 22.0) -> int:
    """Count background-coloured pixels that have floor above *and* below them.

    Sky above floor is the normal layout; a sliver of sky colour sandwiched between floor is the
    signature of the artefact at the mirror/floor seam.
    """
    bg = background_colour(srgb8)
    is_background = np.sqrt(((srgb8 - bg) ** 2).sum(axis=-1)) < tolerance
    r, b = srgb8[..., 0], srgb8[..., 2]
    is_floor = (r > b + 8) & (r > 70)

    above = np.zeros_like(is_background)
    above[gap:, :] = is_floor[:-gap, :]
    below = np.zeros_like(is_background)
    below[:-gap, :] = is_floor[gap:, :]

    return int((is_background & above & below).sum())


# --------------------------------------------------------------------------- table 1: shadows


def build_shadow_table(images_dir: Path) -> pd.DataFrame:
    rows = []
    for preset in PRESETS:
        srgb = {r: load_srgb8(images_dir, preset, r) for r in RENDERERS}
        lum = {r: luminance(srgb_to_linear(srgb[r])) for r in RENDERERS}

        floor = floor_mask(srgb["engine"], lum["engine"], lum["povray"])
        ratio = np.where(floor, lum["engine"] / np.maximum(lum["povray"], 1e-9), np.nan)
        shadow = floor & (ratio >= SHADOW_RATIO_THRESHOLD)
        lit = floor & (ratio < SHADOW_RATIO_THRESHOLD)

        if shadow.sum() < 100:
            continue

        for renderer in RENDERERS:
            lit_mean = float(lum[renderer][lit].mean())
            shadow_mean = float(lum[renderer][shadow].mean())
            rows.append(
                {
                    "preset": preset,
                    "renderer": renderer,
                    "shadow_px_pct": 100.0 * shadow.sum() / floor.sum(),
                    "lit_luminance": lit_mean,
                    "shadow_luminance": shadow_mean,
                    "shadow_over_lit": shadow_mean / lit_mean,
                    "predicted_shadow_over_lit": {
                        "engine": PRED_ENGINE_CONTRAST,
                        "povray": PRED_POVRAY_CONTRAST,
                    }.get(renderer, float("nan")),
                    "ratio_vs_povray_lit": float(ratio[lit].mean()) if renderer == "engine"
                    else float("nan"),
                    "ratio_vs_povray_shadow": float(ratio[shadow].mean()) if renderer == "engine"
                    else float("nan"),
                }
            )

    return pd.DataFrame(rows)


# --------------------------------------------------------------------------- table 2: agreement


def _rmse_psnr(x: np.ndarray, y: np.ndarray) -> tuple[float, float]:
    mse = float(((x - y) ** 2).mean())
    return float(np.sqrt(mse)), float(10.0 * np.log10(1.0 / max(mse, 1e-12)))


def build_agreement_table(images_dir: Path) -> pd.DataFrame:
    rows = []
    for preset in PRESETS:
        srgb = {r: load_srgb8(images_dir, preset, r) for r in RENDERERS}
        linear = {r: srgb_to_linear(srgb[r]) for r in RENDERERS}

        has_background = preset not in PRESETS_WITHOUT_BACKGROUND

        row: dict[str, object] = {"preset": preset}
        for a, b in (("engine", "povray"), ("engine", "blender"), ("povray", "blender")):
            rmse, psnr = _rmse_psnr(linear[a], linear[b])
            row[f"rmse_{a}_{b}"] = rmse
            row[f"psnr_{a}_{b}"] = psnr
        for renderer in RENDERERS:
            row[f"background_{renderer}"] = (
                background_colour(srgb[renderer]) if has_background else None
            )
        rows.append(row)

    return pd.DataFrame(rows)


# --------------------------------------------------------------------------- table 3: seam


def build_seam_table(images_dir: Path) -> pd.DataFrame:
    rows = []
    for preset in PRESETS:
        if preset in PRESETS_WITHOUT_BACKGROUND:
            continue
        row: dict[str, object] = {"preset": preset}
        counts = {}
        for renderer in RENDERERS:
            counts[renderer] = seam_pixels(load_srgb8(images_dir, preset, renderer))
            row[f"seam_px_{renderer}"] = counts[renderer]
        offline_max = max(counts["povray"], counts["blender"])
        row["engine_over_offline"] = (
            counts["engine"] / offline_max if offline_max > 0 else float("nan")
        )
        rows.append(row)

    return pd.DataFrame(rows)


# --------------------------------------------------------------------------- export


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


def _fmt(value: float, decimals: int = MD_DECIMALS) -> str:
    return "" if pd.isna(value) else f"{value:.{decimals}f}"


def write_shadow_outputs(df: pd.DataFrame, output_prefix: Path) -> tuple[Path, Path]:
    export = df.copy()
    export["renderer"] = export["renderer"].map(RENDERER_LABELS)
    export["shadow_px_pct"] = export["shadow_px_pct"].map(lambda v: f"{v:.1f}")
    for col in ("lit_luminance", "shadow_luminance"):
        export[col] = export[col].map(lambda v: _fmt(v))
    for col in ("shadow_over_lit", "predicted_shadow_over_lit",
                "ratio_vs_povray_lit", "ratio_vs_povray_shadow"):
        export[col] = export[col].map(lambda v: _fmt(v, DECIMALS))

    export.columns = [
        "Preset", "Renderer", "Piksele w cieniu [%]",
        "Luminancja w świetle", "Luminancja w cieniu",
        "Cień/światło", "Cień/światło — przewidziane",
        "Silnik/POV-Ray w świetle", "Silnik/POV-Ray w cieniu",
    ]

    return _write_csv_and_md(
        export,
        output_prefix,
        "Odpowiedź na cień: silnik vs renderery offline",
        [
            "Luminancja Rec. 709 w przestrzeni liniowej (PNG-i są zakodowane w sRGB, więc przed "
            "liczeniem stosunków są dekodowane).",
            "Maska cienia wyprowadzona z samego stosunku luminancji silnik/POV-Ray "
            f"(próg {SHADOW_RATIO_THRESHOLD}), a nie z ręcznie wskazanych prostokątów — rozkład "
            "jest wyraźnie dwumodalny, więc podział jest obiektywny i powtarzalny. Ta sama maska "
            "jest potem stosowana do wszystkich trzech rendererów.",
            "",
            "Kolumna „przewidziane\" pochodzi ze stałych shadera "
            "(`shaders/ray_tracing/default.slang`): `kAmbientStrength = 0.12`, "
            f"`kShadowStrength = 0.2`, dla podłogi `N·L = 1/√2 = {N_DOT_L:.4f}`. "
            f"Silnik: `(0.12 + 0.2·N·L) / (0.12 + N·L)` = {PRED_ENGINE_CONTRAST:.3f}. "
            f"POV-Ray, gdzie w cieniu zostaje sam ambient: `0.12 / (0.12 + N·L)` = "
            f"{PRED_POVRAY_CONTRAST:.3f}.",
            "",
            f"Przewidywany stosunek jasności samych cieni silnik/POV-Ray: "
            f"{PRED_ENGINE_SHADOW:.4f} / {PRED_POVRAY_SHADOW:.2f} = {PRED_SHADOW_RATIO:.3f}.",
            "",
            "W `reflection_closeup` większość kadru zajmuje lustro, więc piksele „podłogi\" to "
            "w istocie jej odbicie (przy zmieszaniu 0.98 niosące to samo cieniowanie).",
        ],
    )


def write_agreement_outputs(df: pd.DataFrame, output_prefix: Path) -> tuple[Path, Path]:
    export = df.copy()
    for col in export.columns:
        if col.startswith("rmse_"):
            export[col] = export[col].map(lambda v: _fmt(v))
        elif col.startswith("psnr_"):
            export[col] = export[col].map(lambda v: _fmt(v, 2))
        elif col.startswith("background_"):
            export[col] = export[col].map(
                lambda v: "" if v is None else ", ".join(f"{c:.0f}" for c in v)
            )

    export.columns = [
        "Preset",
        "RMSE silnik–POV-Ray", "PSNR silnik–POV-Ray [dB]",
        "RMSE silnik–Blender", "PSNR silnik–Blender [dB]",
        "RMSE POV-Ray–Blender", "PSNR POV-Ray–Blender [dB]",
        "Tło silnik (sRGB)", "Tło POV-Ray (sRGB)", "Tło Blender (sRGB)",
    ]

    return _write_csv_and_md(
        export,
        output_prefix,
        "Zgodność obrazów między rendererami",
        [
            "RMSE i PSNR liczone na całym kadrze w przestrzeni liniowej, na parach obrazów o "
            "identycznych wymiarach (w obrębie presetu są pixel-aligned).",
            "Tło = średnia z narożnika 40×40 px. W `shadow_closeup` kamera patrzy w podłogę i tła "
            "nie ma w kadrze, dlatego kolumny tła są tam puste.",
            "",
            "Kluczowy odczyt: wysokie PSNR silnik–POV-Ray pokazuje, że kalibracja materiałów i "
            "gammy jest poprawna. Niskie PSNR silnik–Blender w kadrach z niebem, przy wyraźnie "
            "wyższym w `shadow_closeup` (gdzie nieba prawie nie ma), wskazuje, że rozjazd z "
            "Blenderem wynika z ekspozycji i tła, nie z geometrii.",
        ],
    )


def write_seam_outputs(df: pd.DataFrame, output_prefix: Path) -> tuple[Path, Path]:
    export = df.copy()
    export["engine_over_offline"] = export["engine_over_offline"].map(
        lambda v: _fmt(v, 1) + "×" if not pd.isna(v) else ""
    )

    export.columns = [
        "Preset", "Silnik [px]", "POV-Ray [px]", "Blender [px]", "Silnik / offline",
    ]

    return _write_csv_and_md(
        export,
        output_prefix,
        "Artefakt szwu na krawędzi lustra",
        [
            "Liczba pikseli w kolorze tła danego obrazu (odległość euklidesowa < 22 od średniej "
            "z narożnika 40×40), które mają piksel podłogi 15 wierszy wyżej **i** niżej. Niebo "
            "nad podłogą to normalny układ; pasek koloru nieba wciśnięty między podłogę to "
            "sygnatura artefaktu w miejscu, gdzie podstawa lustra styka się z podłogą.",
            "",
            "`shadow_closeup` pominięty — w kadrze nie ma tła, więc próbka z narożnika trafiłaby "
            "w podłogę i detektor zwróciłby setki tysięcy fałszywych trafień.",
            "",
            "Artefakt występuje we wszystkich trzech rendererach, co wskazuje na degenerację "
            "geometrii sceny (podstawa lustra leży dokładnie w płaszczyźnie podłogi, `y = 0`), a "
            "nie na błąd samego silnika. Silnik pokazuje go mocniej, bo renderuje jedną próbkę "
            "na piksel bez antyaliasingu.",
        ],
    )


def compute_and_write(images_dir: Path,
                      output_dir: Path) -> tuple[pd.DataFrame, pd.DataFrame, pd.DataFrame]:
    shadows = build_shadow_table(images_dir)
    agreement = build_agreement_table(images_dir)
    seam = build_seam_table(images_dir)

    write_shadow_outputs(shadows, output_dir / "cienie")
    write_agreement_outputs(agreement, output_dir / "zgodnosc")
    write_seam_outputs(seam, output_dir / "szew_lustra")

    return shadows, agreement, seam
