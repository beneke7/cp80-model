#!/usr/bin/env python3
"""Replay the model demo score from the sparse reference-sample pack."""
import csv
import os
import sys
from fractions import Fraction
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly


SR = 44100
TARGET_PEAK = 0.89
# Midpoints of the SFZ velocity ranges: PP 1-49, MP 50-81, F 82-103, FF 104-127.
VELS = {"PP": 25.0 / 127.0, "MP": 65.5 / 127.0,
        "F": 92.5 / 127.0, "FF": 115.5 / 127.0}
NAMES = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")


def parse(path):
    note, name, tag = path.stem.split("-", 2)
    return int(note), name, tag


def score():
    events = []

    def chord(t, duration, notes, velocity):
        for i, note in enumerate(notes):
            events.append((t + i * 0.012, duration, note, velocity))

    chord(0.20, 2.0, (48, 55, 64, 67, 72, 76), 0.55)
    chord(2.35, 2.0, (45, 52, 64, 69, 72, 74), 0.50)
    chord(4.50, 2.0, (46, 53, 62, 65, 69, 77), 0.46)
    chord(6.65, 2.6, (43, 50, 59, 65, 67, 74), 0.42)
    for i, note in enumerate((72, 76, 79, 84, 88, 91, 84, 79)):
        events.append((6.90 + i * 0.16, 0.40, note, 0.47))
    return events


def nearest_tag(velocity):
    return min(VELS, key=lambda tag: abs(VELS[tag] - velocity))


def main():
    ref_dir = Path(os.environ.get("CP80_REFERENCE_DIR", "../GregSullivan.E-Pianos/CP80/Samples"))
    sample_dir = Path(os.environ.get("CP80_DEMO_SAMPLE_DIR", str(ref_dir)))
    out_path = Path(sys.argv[1] if len(sys.argv) > 1 else "out/demo-reference.wav")
    inverse_csv = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    files = []
    for path in sorted(sample_dir.glob("*.flac")):
        try:
            note, name, tag = parse(path)
        except ValueError:
            continue
        x, sr = sf.read(path, always_2d=False)
        x = np.asarray(x, dtype=np.float64)
        if x.ndim > 1:
            x = x.mean(axis=1)
        files.append((path, note, name, tag, x, sr))
    if not files:
        raise SystemExit(f"no reference samples found in {ref_dir}")

    reference_peak = {}
    model_peaks = {}
    if inverse_csv:
        with inverse_csv.open(newline="") as f:
            for row in csv.DictReader(f):
                reference_peak[row["file"]] = float(row["reference_peak"])
                model_peaks.setdefault(int(row["note"]), []).append(
                    (VELS[row["velocity"]], float(row["model_peak_before"]))
                )

    def model_peak(note, velocity):
        points = model_peaks.get(note)
        if not points:
            nearest = min(model_peaks, key=lambda n: abs(n - note))
            points = model_peaks[nearest]
        points = sorted(points)
        xs = np.array([p[0] for p in points])
        ys = np.log(np.maximum([p[1] for p in points], 1e-30))
        return float(np.exp(np.interp(velocity, xs, ys)))

    cache = {}
    sources = []

    def choose(note, velocity):
        want = nearest_tag(velocity)
        return min(files, key=lambda row: (
            abs(row[1] - note),
            0 if row[3] == want else 1,
            abs(VELS[row[3]] - velocity),
        ))

    def voice(note, velocity):
        source = choose(note, velocity)
        path, source_note, _, tag, x, source_sr = source
        key = (path.name, note)
        if key not in cache:
            ratio = 2.0 ** ((note - source_note) / 12.0)
            frac = Fraction(1.0 / ratio).limit_denominator(1000)
            y = resample_poly(x, frac.numerator, frac.denominator)
            cache[key] = y
        y = cache[key]
        if reference_peak:
            # Match the model's actual score velocity, not merely the
            # nearest recorded layer used as the source sample.
            y = y * (model_peak(source_note, velocity) / reference_peak[path.name])
        sources.append((note, velocity, path.name, source_note, tag))
        return y

    total = int(11.0 * SR)
    out = np.zeros(total, dtype=np.float64)
    release = int(0.25 * SR)
    for start, duration, note, velocity in score():
        y = voice(note, velocity)
        a = int(start * SR)
        if a >= total:
            continue
        count = min(len(y), total - a)
        env = np.ones(count)
        r0 = min(count, int(duration * SR))
        r1 = min(count, r0 + release)
        env[r0:r1] = np.linspace(1.0, 0.0, r1 - r0, endpoint=False)
        env[r1:] = 0.0
        out[a:a + count] += y[:count] * env

    peak = float(np.max(np.abs(out)))
    if peak:
        out *= TARGET_PEAK / peak
    out_path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(out_path, out.astype(np.float32), SR, subtype="PCM_24")
    with out_path.with_suffix(".sources.csv").open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(("target_note", "velocity", "source_file", "source_note", "source_velocity"))
        writer.writerows(sources)
    print(f"wrote {out_path} peak={np.max(np.abs(out)):.6f}")


if __name__ == "__main__":
    main()
