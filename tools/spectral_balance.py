#!/usr/bin/env python3
"""Compare attack/sustain band energy and early decay by note."""
import argparse
import os

import numpy as np
import soundfile as sf
from scipy.signal import butter, hilbert, sosfiltfilt


BANDS = ((100, 1000), (1000, 2000), (2000, 3000),
         (3000, 4000), (4000, 6000), (6000, 9000))
WINDOWS = (("attack", 0.0, 0.03), ("sustain", 0.20, 0.50))
NAMES = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")


def load(path):
    x, sr = sf.read(path)
    x = np.asarray(x, dtype=float)
    return (x.mean(axis=1) if x.ndim > 1 else x), sr


def note_name(note):
    return f"{NAMES[note % 12]}{note // 12 - 1}"


def _spectrum(x, sr, start, end):
    a, b = int(start * sr), min(len(x), int(end * sr))
    if b <= a + 16:
        return np.empty(0), np.empty(0)
    w = np.hanning(b - a)
    # Keep the resolution fixed between the 30 ms attack and 300 ms sustain
    # windows; otherwise the H1 reference changes with the window length.
    nfft = 1 << 18
    X = np.abs(np.fft.rfft(x[a:b] * w, nfft)) * (2.0 / max(w.sum(), 1e-30))
    return np.fft.rfftfreq(nfft, 1.0 / sr), X


def h1_peak(x, sr, note):
    f0 = 440.0 * 2.0 ** ((note - 69) / 12.0)
    f, X = _spectrum(x, sr, 0.0, min(0.03, len(x) / sr))
    band = (f >= 0.75 * f0) & (f <= 1.25 * f0)
    return float(np.max(X[band])) if np.any(band) else 1e-30


def band_levels(x, sr, note):
    h1 = h1_peak(x, sr, note)
    out = {}
    for label, start, end in WINDOWS:
        f, X = _spectrum(x, sr, start, end)
        for lo, hi in BANDS:
            band = (f >= lo) & (f < hi)
            p = float(np.sum(X[band] ** 2)) if np.any(band) else 0.0
            out[(label, lo, hi)] = 10.0 * np.log10(max(p, 1e-30) / (h1 * h1))
    return out


def band_sigmas(x, sr, start=0.05, end=0.30):
    """Return amplitude-decay sigma for each band, in 1/s.

    The fit is intentionally short and floor-limited: high partials are not
    extrapolated through the recording's noise tail.
    """
    end = min(end, len(x) / sr)
    if end <= start + 0.04:
        return [float("nan")] * len(BANDS)
    result = []
    t = np.arange(int(start * sr), int(end * sr)) / sr
    for lo, hi in BANDS:
        sos = butter(6, (lo, hi), btype="bandpass", fs=sr, output="sos")
        z = sosfiltfilt(sos, x)
        env = np.abs(hilbert(z))[int(start * sr):int(end * sr)]
        # Smooth the carrier ripple before taking logs; keep only signal above
        # a relative floor so the fit cannot turn the file noise into damping.
        hop = max(1, int(0.002 * sr))
        n = len(env) // hop
        env = env[:n * hop].reshape(n, hop).mean(axis=1)
        tt = t[:n * hop:hop] + 0.001
        peak = float(np.max(env)) if len(env) else 0.0
        keep = env > peak * 10.0 ** (-45.0 / 20.0)
        if keep.sum() < 8:
            result.append(float("nan"))
            continue
        result.append(float(-np.polyfit(tt[keep], np.log(np.maximum(env[keep], 1e-30)), 1)[0]))
    return result


def measure(path, note):
    x, sr = load(path)
    return band_levels(x, sr, note), band_sigmas(x, sr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--notes", nargs="+", type=int, default=(27, 42))
    ap.add_argument("--reference-dir", default=os.environ.get(
        "CP80_REFERENCE_DIR", "../GregSullivan.E-Pianos/CP80/Samples"))
    ap.add_argument("--model-dir", default=os.environ.get("CP80_MODEL_DIR", "out/model-lib"))
    args = ap.parse_args()

    for note in args.notes:
        name = f"{note:03d}-{note_name(note)}-F.flac"
        paths = (("real", os.path.join(args.reference_dir, name)),
                 ("model", os.path.join(args.model_dir, name)))
        print(f"\n{note} {note_name(note)}")
        print("source       " + " ".join(f"{lo:g}-{hi:g}" for lo, hi in BANDS))
        for label, path in paths:
            levels, sigmas = measure(path, note)
            attack = [levels[("attack", lo, hi)] for lo, hi in BANDS]
            sustain = [levels[("sustain", lo, hi)] for lo, hi in BANDS]
            print(f"{label:6s} attack " + " ".join(f"{v:+6.1f}" for v in attack))
            print(f"{label:6s} sustain" + " ".join(f"{v:+6.1f}" for v in sustain))
            print(f"{label:6s} sigma  " + " ".join(f"{v:6.2f}" for v in sigmas))


if __name__ == "__main__":
    main()
