#!/usr/bin/env python3
"""Measure the short high-frequency attack separately from the decay."""
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import soundfile as sf


NOTES = (27, 42, 46, 50, 57, 60, 65, 72, 80)
VEL = 92.5 / 127.0  # midpoint of the SFZ F layer (MIDI velocities 82--103)
WIDTHS = (0.003, 0.010, 0.030, 0.074)
LPS = (3000.0, 6000.0, 12000.0)


def note_name(note):
    names = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    return f"{names[note % 12]}{note // 12 - 1}"


def band_db(x, sr, start, length=1024):
    segment = np.zeros(length, dtype=float)
    end = min(start + length, len(x))
    segment[:end - start] = x[start:end]
    X = np.abs(np.fft.rfft(segment * np.hanning(length))) ** 2
    f = np.fft.rfftfreq(length, 1.0 / sr)
    low = X[(f >= 100.0) & (f < 2000.0)].sum()
    mid = X[(f >= 3000.0) & (f < 8000.0)].sum()
    high = X[(f >= 8000.0) & (f < 18000.0)].sum()
    return 10.0 * np.log10(np.maximum([mid, high], 1e-30) / max(low, 1e-30))


def measure(x, sr):
    return np.concatenate([band_db(x, sr, int(ms * sr / 1000.0)) for ms in (0, 10, 30, 80)])


def ref_dir():
    return os.environ.get("CP80_REFERENCE_DIR", "../GregSullivan.E-Pianos/CP80/Samples")


def main():
    refdir = ref_dir()
    zscale = float(os.environ.get("CP80_ATTACK_Z", "3"))
    damping = os.environ.get("CP80_ATTACK_DAMPING", ".5")
    kscale = os.environ.get("CP80_ATTACK_KSCALE", ".5")
    exponent = os.environ.get("CP80_ATTACK_P", "2.75")
    facing = os.environ.get("CP80_ATTACK_FACING", "0")
    tau_ms = os.environ.get("CP80_ATTACK_TAU_MS", "0.1")
    print("window start ms: 0,10,30,80; each pair is 3-8 kHz / low and 8-18 kHz / low")
    for note in NOTES:
        path = os.path.join(refdir, f"{note:03d}-{note_name(note)}-F.flac")
        ref, sr = sf.read(path)
        ref = np.asarray(ref, dtype=float)
        if ref.ndim > 1:
            ref = ref.mean(axis=1)
        print(f"\nnote={note} reference", " ".join(f"{v:+5.1f}" for v in measure(ref, sr)))

        def one(job):
            width, lp = job
            env = os.environ.copy()
            env.update({
                "CP80_WAVE_CONTACT": "1",
                "CP80_WAVE_Z": f"{zscale:.8g}",
                "CP80_HAMMER_DAMPING": damping,
                "CP80_HAMMER_SCALE": kscale,
                "CP80_HAMMER_P": exponent,
                "CP80_HAMMER_FACING": facing,
                "CP80_HAMMER_TAU_MS": tau_ms,
                "CP80_HAMMER_WIDTH": f"{width:.8g}",
                "CP80_PICKUP_LP": f"{lp:.8g}",
            })
            p = subprocess.run(["./build/analyze", str(note), str(VEL), ".15"],
                               capture_output=True, env=env, check=True)
            x = np.frombuffer(p.stdout, dtype=np.float32).astype(float)
            return width, lp, measure(x, 48000)

        jobs = [(width, lp) for width in WIDTHS for lp in LPS]
        with ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(one, jobs))
        for width, lp, values in results:
            print(f"width={width:.3f} lp={lp:5.0f}", " ".join(f"{v:+5.1f}" for v in values))


if __name__ == "__main__":
    main()
