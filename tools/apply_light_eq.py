#!/usr/bin/env python3
"""Apply the deliberately small, chord-derived demo/output EQ."""
import argparse
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path)
    ap.add_argument("output", type=Path)
    ap.add_argument("--peak", type=float, default=0.89)
    args = ap.parse_args()

    x, sr = sf.read(args.input, always_2d=False)
    x = np.asarray(x, dtype=np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    if len(x) < sr:
        raise SystemExit("light EQ needs at least one second of audio")

    def band(lo, hi):
        return sosfiltfilt(butter(4, [lo, hi], btype="bandpass", fs=sr, output="sos"), x)

    low = sosfiltfilt(butter(4, 250.0, btype="lowpass", fs=sr, output="sos"), x)
    mid = band(600.0, 1600.0)
    high = band(2500.0, 6500.0)
    y = (x
         + (10.0 ** (0.55 / 20.0) - 1.0) * low
         + (10.0 ** (-0.75 / 20.0) - 1.0) * mid
         + (10.0 ** (1.20 / 20.0) - 1.0) * high)
    peak = float(np.max(np.abs(y)))
    if not np.isfinite(peak) or peak <= 0.0:
        raise SystemExit("invalid EQ output")
    y *= args.peak / peak
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sf.write(args.output, y.astype(np.float32), sr, subtype="PCM_24")
    print(f"wrote {args.output} peak={np.max(np.abs(y)):.6f} bands=+0.55/-0.75/+1.20 dB")


if __name__ == "__main__":
    main()
