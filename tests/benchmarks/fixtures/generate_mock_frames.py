"""Generate mock per-frame CSV traces matching the diagnostic suite's output format.

Used to exercise the frame-split charts without a GPU run. The numbers encode the hypothesis under
test — at light raster load the wall-clock spikes come from present/acquire while GPU time stays
flat; under heavy load both tails move together — so a chart built from these should look like the
expected result, not like proof of it.
"""

from __future__ import annotations

import argparse
import random
from pathlib import Path

# backend, stress_count, mean wall ms, mean gpu ms, spike probability, spike wall multiplier
# Mirrors the diagnostic.configs list in scenes/stress_test.json.
CONFIGS = [
    ("raster", 10000, 0.2184, 0.1902, 0.012, 9.0),
    ("raster", 100000, 1.2270, 1.1500, 0.010, 2.1),
    ("rt_full", 10000, 16.8000, 16.1000, 0.010, 1.9),
    ("rt_full", 100000, 159.2000, 45.0000, 0.010, 1.4),
]

FRAME_COUNT = 6000
RUNS_PER_CONFIG = 3
SESSION = "2026-07-28T00-00-00"
RNG_SEED = 7


def _write_trace(path: Path, backend: str, stress: int, run: int, mean_wall: float,
                 mean_gpu: float, spike_p: float, spike_mult: float,
                 rng: random.Random) -> None:
    gpu_bound = mean_gpu / mean_wall > 0.9 and stress >= 100000 or backend != "raster"

    lines = [
        f"# backend={backend} stress_count={stress} run={run}",
        "# gpu=NVIDIA GeForce RTX 4070 SUPER (mock) present_mode=mailbox",
        "frame,serial,wall_clock_time_ms,gpu_time_ms,cpu_record_ms,cpu_submit_ms,"
        "cpu_fence_wait_ms,cpu_acquire_ms,cpu_present_ms",
    ]

    for i in range(FRAME_COUNT):
        wall = rng.gauss(mean_wall, mean_wall * 0.04)
        gpu = rng.gauss(mean_gpu, mean_gpu * 0.02)

        spike = rng.random() < spike_p
        if spike:
            wall *= spike_mult
            # A GPU-bound configuration stretches the GPU too; a CPU-bound one does not.
            if gpu_bound:
                gpu *= spike_mult * 0.92

        wall = max(wall, 0.001)
        gpu = max(min(gpu, wall * 0.995), 0.001)

        residual = wall - gpu
        if gpu_bound:
            fence = residual * 0.70
        else:
            fence = residual * 0.04
        rest = max(residual - fence, 0.0)

        present = rest * (0.72 if spike and not gpu_bound else 0.30)
        acquire = rest * (0.14 if spike and not gpu_bound else 0.30)
        record = rest * 0.26
        submit = max(rest - present - acquire - record, 0.0)

        lines.append(
            f"{i},{i},{wall:.5f},{gpu:.5f},{record:.5f},{submit:.5f},"
            f"{fence:.5f},{acquire:.5f},{present:.5f}"
        )

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parent / "mock_frames",
        help="Directory to write frames_*.csv into",
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    rng = random.Random(RNG_SEED)
    written = 0
    for backend, stress, mean_wall, mean_gpu, spike_p, spike_mult in CONFIGS:
        for run in range(1, RUNS_PER_CONFIG + 1):
            name = f"frames_{backend}_{stress}_run{run}_{SESSION}.csv"
            _write_trace(args.output / name, backend, stress, run, mean_wall, mean_gpu,
                         spike_p, spike_mult, rng)
            written += 1

    print(f"Wrote {written} mock traces -> {args.output}")


if __name__ == "__main__":
    main()
