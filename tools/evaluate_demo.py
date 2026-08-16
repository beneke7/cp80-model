#!/usr/bin/env python3
"""Compare the assembled musical demo, not just isolated samples."""
import argparse
import csv
from math import gcd
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, hilbert, resample_poly, sosfiltfilt


TARGET_SR = 44100
WINDOWS = (
    ("Cmaj9", 0.65, 1.75),
    ("Am11", 2.80, 3.90),
    ("Bbmaj7sharp11", 4.95, 6.05),
    ("G7sus", 7.10, 8.15),
)
BANDS = ((20, 60), (60, 120), (120, 220), (120, 2000),
         (2000, 4000), (4000, 8000))
MOD_BANDS = ((300, 3000), (1000, 5000), (3000, 8000))
EPS = 1e-30


def load(path):
    x, sr = sf.read(path, always_2d=False)
    x = np.asarray(x, dtype=np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    if sr != TARGET_SR:
        g = gcd(sr, TARGET_SR)
        x = resample_poly(x, TARGET_SR // g, sr // g)
        sr = TARGET_SR
    return x, sr


def rms_db(x):
    return 20.0 * np.log10(max(float(np.sqrt(np.mean(x * x))), EPS))


def filtered(x, sr, lo, hi):
    return sosfiltfilt(butter(6, (lo, hi), btype="bandpass", fs=sr, output="sos"), x)


def measure(x, sr):
    band_cache = {band: filtered(x, sr, *band) for band in BANDS + MOD_BANDS}
    result = {}
    for section, start, end in WINDOWS:
        a, b = int(start * sr), min(len(x), int(end * sr))
        for lo, hi in BANDS:
            result[(section, f"rms_{lo:g}_{hi:g}_db")] = rms_db(
                band_cache[(lo, hi)][a:b])
        for lo, hi in MOD_BANDS:
            env = np.abs(hilbert(band_cache[(lo, hi)][a:b]))
            edb = 20.0 * np.log10(np.maximum(env, EPS))
            t = np.arange(len(edb), dtype=np.float64) / sr
            trend = np.polyval(np.polyfit(t, edb, 1), t)
            result[(section, f"mod_{lo:g}_{hi:g}_db_rms")] = float(
                np.std(edb - trend))

        z = x[a:b] * np.hanning(max(b - a, 2))
        spectrum = np.abs(np.fft.rfft(z)) ** 2
        freq = np.fft.rfftfreq(len(z), 1.0 / sr)
        keep = (freq >= 1000.0) & (freq < 8000.0)
        power = np.maximum(spectrum[keep], EPS)
        result[(section, "flatness_1k_8k_db")] = float(
            10.0 * np.log10(np.exp(np.mean(np.log(power))) / np.mean(power)))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", default="out/demo-model-final-no-eq.wav")
    parser.add_argument("--reference", default="out/demo-reference-inverse-volume.wav")
    parser.add_argument("--output", default="out/demo-eval.csv")
    args = parser.parse_args()

    model, sr = load(Path(args.model))
    reference, ref_sr = load(Path(args.reference))
    if sr != ref_sr:
        raise SystemExit(f"sample-rate mismatch after resampling: {sr} != {ref_sr}")
    model_metrics = measure(model, sr)
    reference_metrics = measure(reference, ref_sr)
    keys = sorted(set(model_metrics) & set(reference_metrics))
    rows = []
    for section, metric in keys:
        m, r = model_metrics[(section, metric)], reference_metrics[(section, metric)]
        if not np.isfinite(m) or not np.isfinite(r):
            raise SystemExit(f"non-finite demo metric: {section} {metric}")
        rows.append({"section": section, "metric": metric,
                     "model": f"{m:.9g}", "reference": f"{r:.9g}",
                     "delta": f"{m - r:.9g}"})

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=("section", "metric", "model",
                                                      "reference", "delta"))
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {len(rows)} demo metrics to {output}")
    for section, _, _ in WINDOWS:
        values = [float(row["delta"]) for row in rows
                  if row["section"] == section and row["metric"].startswith("mod_")]
        print(f"{section}: mean modulation delta {np.mean(values):+.2f} dB RMS")


if __name__ == "__main__":
    main()
