#!/usr/bin/env python3
"""Echo-trail test using scene-generator rendering (LUT-based polar→Cartesian).

Matches imsel-lab/radar-scene-augmentation scene-generator/src/main.rs logic:
  - 720 regularised pulses, 868 range bins
  - 1735×1735 circular PPI via precomputed LUT
  - North-up atan2(dx, dy) convention
"""
import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image

NUM_RANGE_BINS = 868
NUM_PULSES = 720
IMAGE_SIZE = 1735
ANGLE_TICKS_PER_ROTATION = 8192.0


def _ticks_to_rad(ticks: int) -> float:
    return (ticks / ANGLE_TICKS_PER_ROTATION) * 2.0 * math.pi


def _read_csv(csv_path: Path):
    """Read a radar CSV and return list of (angle_rad, echoes[868])."""
    pulses = []
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        next(reader, None)  # skip header
        for row in reader:
            if len(row) < 6:
                continue
            angle_ticks = int(row[4])
            echoes = np.zeros(NUM_RANGE_BINS, dtype=np.uint8)
            n = min(len(row) - 5, NUM_RANGE_BINS)
            for j in range(n):
                v = row[5 + j]
                if v:
                    echoes[j] = min(int(v), 255)
            pulses.append((_ticks_to_rad(angle_ticks), echoes))
    pulses.sort(key=lambda p: p[0])
    return pulses


def _regularised_echo_array(pulses, max_gap_deg: float = 1.0) -> np.ndarray:
    """Resample to NUM_PULSES×NUM_RANGE_BINS grid (nearest-pulse, max-gap threshold)."""
    echo = np.zeros((NUM_PULSES, NUM_RANGE_BINS), dtype=np.uint8)
    if not pulses:
        return echo
    angles = np.array([p[0] for p in pulses])
    angle_step = 2.0 * math.pi / NUM_PULSES
    max_gap_rad = math.radians(max_gap_deg)
    for i in range(NUM_PULSES):
        target = i * angle_step
        diff = np.abs(angles - target)
        diff = np.minimum(diff, 2.0 * math.pi - diff)
        idx = int(np.argmin(diff))
        if diff[idx] <= max_gap_rad:
            echo[i] = pulses[idx][1]
    return echo


def _build_render_lut():
    """Build LUT mapping each (y,x) pixel to (pulse_idx, bin_idx) or invalid."""
    size = IMAGE_SIZE
    center = size / 2.0
    ys, xs = np.mgrid[0:size, 0:size]
    dx = xs.astype(np.float64) - center
    dy = center - ys.astype(np.float64)
    r = np.sqrt(dx * dx + dy * dy)
    r_norm = r / center

    valid = r_norm <= 1.0
    theta = np.arctan2(dx, dy)  # north-up: atan2(dx, dy)
    theta[theta < 0] += 2.0 * math.pi

    pulse_idx = ((theta / (2.0 * math.pi)) * NUM_PULSES).astype(np.int32) % NUM_PULSES
    bin_idx = (r_norm * NUM_RANGE_BINS).astype(np.int32)
    bin_idx = np.clip(bin_idx, 0, NUM_RANGE_BINS - 1)

    return valid, pulse_idx, bin_idx


# Pre-build LUT once at import time
_LUT_VALID, _LUT_PULSE, _LUT_BIN = _build_render_lut()


def render_mono_from_csv(csv_path: Path) -> np.ndarray:
    """Render a grayscale PPI image from a radar CSV file."""
    pulses = _read_csv(csv_path)
    echo = _regularised_echo_array(pulses)
    image = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=np.uint8)
    image[_LUT_VALID] = echo[_LUT_PULSE[_LUT_VALID], _LUT_BIN[_LUT_VALID]]
    return image


def main() -> None:
    p = argparse.ArgumentParser(description="Generate 10 frame echo-trail test from CSV data.")
    p.add_argument("--input-dir", default="data_feb14_2026")
    p.add_argument("--output-dir", default="echo_trail_test_output")
    p.add_argument("--frames", type=int, default=10)
    p.add_argument("--trail-len", type=int, default=6)
    args = p.parse_args()

    input_dir = Path(args.input_dir)
    out_dir = Path(args.output_dir)
    mono_dir = out_dir / "mono"
    trail_dir = out_dir / "trail_rgb"
    mono_dir.mkdir(parents=True, exist_ok=True)
    trail_dir.mkdir(parents=True, exist_ok=True)

    csv_files = sorted(input_dir.glob("*.csv"))[: args.frames]
    if not csv_files:
        raise RuntimeError(f"No CSV files found in {input_dir}")

    decay_step = int(math.ceil(255.0 / float(args.trail_len)))
    trail_g = None
    prev_current_r = None

    for i, csv_file in enumerate(csv_files):
        mono = render_mono_from_csv(csv_file)
        h, w = mono.shape

        if trail_g is None or trail_g.shape != mono.shape:
            trail_g = np.zeros((h, w), dtype=np.uint8)
            prev_current_r = np.zeros((h, w), dtype=np.uint8)

        current_r = np.where(mono > 0, 255, 0).astype(np.uint8)

        # Linear decay over trail length, then add previous-frame red into green history.
        trail_g = np.maximum(trail_g.astype(np.int16) - decay_step, 0).astype(np.uint8)
        trail_g = np.maximum(trail_g, prev_current_r)

        # Replace semantics (no yellow): red wins; green is hidden where red exists.
        g_out = np.where(current_r > 0, 0, trail_g).astype(np.uint8)
        b_out = np.zeros_like(current_r, dtype=np.uint8)
        rgb = np.stack([current_r, g_out, b_out], axis=2)

        Image.fromarray(mono, mode="L").save(mono_dir / f"{i:02d}_{csv_file.stem}.png")
        Image.fromarray(rgb, mode="RGB").save(trail_dir / f"{i:02d}_{csv_file.stem}.png")

        prev_current_r = current_r

    print(f"Generated {len(csv_files)} mono frames in: {mono_dir}")
    print(f"Generated {len(csv_files)} trail RGB frames in: {trail_dir}")
    print(f"trail_len={args.trail_len}, decay_step={decay_step}")


if __name__ == "__main__":
    main()
