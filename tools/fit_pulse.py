#!/usr/bin/env python3
"""Fit a compact asymmetric force pulse through the existing modal bank.

This is the first stage of the hybrid hammer route.  It does not claim to
recover a measured hammer force: it finds the low-dimensional force event that
best explains the early harmonic balance in the reference audio.  The result
is a target for the later viscoelastic hammer fit.
"""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import soundfile as sf


TAGS = ("PP", "MP", "F", "FF")
NOTES = (27, 42, 46, 50, 57, 60, 65, 72, 80)
WEIGHTS = np.array([1.5, 1.5, 1.2, 1.0, 0.6, 0.3, 0.15], dtype=float)


def reference_dir():
    explicit = os.environ.get("CP80_REFERENCE_DIR")
    if explicit:
        return explicit
    sibling = os.path.join("..", "GregSullivan.E-Pianos", "CP80", "Samples")
    return sibling if os.path.isdir(sibling) else "reference"


def name(note):
    names = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    return f"{names[note % 12]}{note // 12 - 1}"


def level(x, sr, first, last, f):
    k = np.arange(first, last, dtype=float)
    w = np.hanning(last - first)
    ph = 2.0 * np.pi * f * k / sr
    z = x[first:last] * w
    return 2.0 * abs(np.sum(z * np.exp(-1j * ph))) / max(np.sum(w), 1e-12)


def reference_metrics(path, f1, B):
    x, sr = sf.read(path)
    x = np.asarray(x, dtype=float)
    if x.ndim > 1:
        x = x.mean(axis=1)
    first = int(0.025 * sr)
    last = min(int(0.240 * sr), len(x))
    vals = []
    for h in range(1, 9):
        f = f1 * h * np.sqrt(1.0 + B * h * h)
        vals.append(level(x, sr, first, last, f))
    vals = np.asarray(vals)
    return 20.0 * np.log10(np.maximum(vals[1:], 1e-30) / max(vals[0], 1e-30))


def candidates(duration, rise, fall, dt, dr, df):
    ts = np.arange(max(1.0, duration - dt), duration + dt + 0.001, max(dt / 4.0, 0.05))
    rs = np.arange(max(0.35, rise - dr), rise + dr + 0.001, max(dr / 3.0, 0.05))
    fs = np.arange(max(0.35, fall - df), fall + df + 0.001, max(df / 3.0, 0.05))
    return [(float(t), float(r), float(f)) for t in ts for r in rs for f in fs]


def run_sweep(note, velocity, params):
    exe = "./build/pulse_sweep"
    payload = "".join(f"{t:.7g} {r:.7g} {f:.7g}\n" for t, r, f in params)
    p = subprocess.run(
        [exe, str(note), str(velocity)], input=payload, text=True,
        capture_output=True, check=True,
    )
    lines = p.stdout.splitlines()
    header = lines[0].split()
    f1 = float(header[2].split("=", 1)[1])
    B = float(header[3].split("=", 1)[1])
    rows = []
    for line in lines[1:]:
        vals = np.fromstring(line, sep=" ")
        if vals.size == 10:
            rows.append(vals)
    return f1, B, np.asarray(rows)


def fit_one(note, tag, velocity, refdir):
    path = os.path.join(refdir, f"{note:03d}-{name(note)}-{tag}.flac")
    if not os.path.exists(path):
        return f"missing {path}"

    coarse = [(t, r, f)
              for t in np.arange(1.5, 8.01, 0.5)
              for r in (0.7, 1.0, 1.5, 2.0, 3.0, 4.0)
              for f in (0.7, 1.0, 1.5, 2.0, 3.0, 4.0)]
    f1, B, _ = run_sweep(note, velocity, coarse)
    target = reference_metrics(path, f1, B)

    def choose(params):
        _, _, rows = run_sweep(note, velocity, params)
        errors = rows[:, 3:] - target[None, :]
        loss = np.sum((errors * WEIGHTS[None, :]) ** 2, axis=1)
        return params[int(np.argmin(loss))], float(np.min(loss))

    best, loss = choose(coarse)
    for dt, dr, df in ((0.5, 0.6, 0.6), (0.18, 0.20, 0.20), (0.06, 0.08, 0.08)):
        best, loss = choose(candidates(*best, dt, dr, df))

    _, _, rows = run_sweep(note, velocity, [best])
    fitted = rows[0, 3:]
    return (note, tag, best, loss, target, fitted, f1, B)


def main():
    refdir = reference_dir()
    jobs = [(note, tag, (25.0 / 127.0, 65.5 / 127.0, 92.5 / 127.0, 115.5 / 127.0)[i], refdir)
            for note in NOTES for i, tag in enumerate(TAGS)]
    with ThreadPoolExecutor(max_workers=min(4, len(jobs))) as pool:
        results = list(pool.map(lambda j: fit_one(*j), jobs))

    print(f"reference_dir={refdir}")
    print("note tag  T_ms rise fall  loss   target H2..H8                         fit H2..H8")
    for result in results:
        if isinstance(result, str):
            print(result)
            continue
        note, tag, best, loss, target, fitted, f1, B = result
        t, r, f = best
        ts = " ".join(f"{v:+5.1f}" for v in target)
        fs = " ".join(f"{v:+5.1f}" for v in fitted)
        print(f"{note:4d} {tag:>2} {t:4.2f} {r:4.2f} {f:4.2f} {loss:6.1f}  {ts}  {fs}")


if __name__ == "__main__":
    main()
