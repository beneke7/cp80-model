#!/usr/bin/env python3
"""Fit the opt-in damped hammer/contact path against Greg attack spectra.

This is stage two of the hybrid route.  The modal bank and output filters stay
unchanged; only the experimental contact parameters move.  The loss uses early
harmonic ratios so per-file recording gain cannot dominate the fit.
"""
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import soundfile as sf


NOTES = (27, 42, 46, 50, 57, 60, 65, 72, 80)
# Midpoints of the SFZ velocity ranges: PP 1-49, MP 50-81, F 82-103, FF 104-127.
VELS = {"PP": 25.0 / 127.0, "MP": 65.5 / 127.0,
        "F": 92.5 / 127.0, "FF": 115.5 / 127.0}
TAGS = tuple(os.environ.get("CP80_HYBRID_TAGS", "F,FF").split(","))
WEIGHTS = np.array([1.5, 1.5, 1.2, 1.0, 0.6, 0.3, 0.15], dtype=float)


def reference_dir():
    explicit = os.environ.get("CP80_REFERENCE_DIR")
    if explicit:
        return explicit
    sibling = os.path.join("..", "GregSullivan.E-Pianos", "CP80", "Samples")
    return sibling if os.path.isdir(sibling) else "reference"


def note_name(note):
    names = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    return f"{names[note % 12]}{note // 12 - 1}"


def model_pitch(note):
    p = subprocess.run(["./build/pulse_sweep", str(note)], capture_output=True,
                       text=True, check=True)
    fields = p.stdout.splitlines()[0].split()
    return float(fields[2].split("=", 1)[1]), float(fields[3].split("=", 1)[1])


def level(x, sr, first, last, f):
    w = np.hanning(last - first)
    k = np.arange(first, last, dtype=float)
    ph = 2.0 * np.pi * f * k / sr
    return 2.0 * abs(np.sum(x[first:last] * w * np.exp(-1j * ph))) / max(np.sum(w), 1e-12)


def ratios(x, sr, f1, B):
    first = int(0.025 * sr)
    last = min(int(0.240 * sr), len(x))
    a = []
    for h in range(1, 9):
        f = f1 * h * np.sqrt(1.0 + B * h * h)
        a.append(level(x, sr, first, last, f))
    a = np.asarray(a)
    return 20.0 * np.log10(np.maximum(a[1:], 1e-30) / max(a[0], 1e-30))


def load_targets(refdir):
    targets = {}
    for note in NOTES:
        f1, B = model_pitch(note)
        for tag in TAGS:
            path = os.path.join(refdir, f"{note:03d}-{note_name(note)}-{tag}.flac")
            if not os.path.exists(path):
                continue
            x, sr = sf.read(path)
            x = np.asarray(x, dtype=float)
            if x.ndim > 1:
                x = x.mean(axis=1)
            targets[(note, tag)] = (ratios(x, sr, f1, B), f1, B)
    return targets


def render_metrics(params, targets):
    z, damping, scale, exponent = params

    def one(job):
        note, tag = job
        env = os.environ.copy()
        env.update({
            "CP80_WAVE_CONTACT": "1",
            "CP80_WAVE_Z": f"{z:.8g}",
            "CP80_HAMMER_DAMPING": f"{damping:.8g}",
            "CP80_HAMMER_SCALE": f"{scale:.8g}",
            "CP80_HAMMER_P": f"{exponent:.8g}",
        })
        p = subprocess.run(["./build/analyze", str(note), str(VELS[tag]), ".28"],
                           capture_output=True, env=env, check=True)
        x = np.frombuffer(p.stdout, dtype=np.float32).astype(float)
        got = ratios(x, 48000, targets[(note, tag)][1], targets[(note, tag)][2])
        err = got - targets[(note, tag)][0]
        return job, got, float(np.sum((err * WEIGHTS) ** 2))

    jobs = list(targets)
    with ThreadPoolExecutor(max_workers=min(4, len(jobs))) as pool:
        results = list(pool.map(one, jobs))
    return results, float(sum(r[2] for r in results))


def main():
    refdir = reference_dir()
    targets = load_targets(refdir)
    current = [5.0, 0.0, 1.0, 2.5]  # Z scale, loss, K scale, exponent
    domains = [
        ("Z", 0, (3.0, 4.0, 5.0, 6.0, 8.0, 10.0)),
        ("damping", 1, (0.0, 0.02, 0.05, 0.10, 0.20, 0.50, 1.0)),
        ("Kscale", 2, (0.50, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0)),
        ("p", 3, (1.5, 2.0, 2.25, 2.5, 2.75, 3.0, 3.5)),
    ]

    base, loss = render_metrics(current, targets)
    print(f"reference_dir={refdir} tags={','.join(TAGS)}")
    print(f"baseline Z={current[0]:g} damping={current[1]:g} Kscale={current[2]:g} p={current[3]:g} loss={loss:.2f}")

    for pass_no in range(2):
        for label, index, values in domains:
            trials = []
            for value in values:
                candidate = current.copy()
                candidate[index] = value
                result, trial_loss = render_metrics(candidate, targets)
                trials.append((trial_loss, candidate, result))
            best_loss, best, best_result = min(trials, key=lambda x: x[0])
            current, loss = best, best_loss
            print(f"pass={pass_no + 1} {label}={current[index]:g} "
                  f"Z={current[0]:g} damping={current[1]:g} Kscale={current[2]:g} "
                  f"p={current[3]:g} loss={loss:.2f}")

    print("\nfinal per-layer metrics: note tag  target H2..H8                         fit H2..H8")
    final, loss = render_metrics(current, targets)
    for (note, tag), got, _ in final:
        target = targets[(note, tag)][0]
        ts = " ".join(f"{v:+5.1f}" for v in target)
        gs = " ".join(f"{v:+5.1f}" for v in got)
        print(f"{note:4d} {tag:>2}  {ts}  {gs}")
    print(f"\nfit Z={current[0]:.6g} damping={current[1]:.6g} Kscale={current[2]:.6g} p={current[3]:.6g} loss={loss:.3f}")


if __name__ == "__main__":
    main()
