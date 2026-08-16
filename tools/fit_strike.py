#!/usr/bin/env python3
"""Estimate strike position from the harmonic comb, without changing anchors.

The smooth hammer spectrum is fitted out; the remaining sharp dips are compared
with |sin(pi*n*x)|.  This is a diagnostic, not an automatic calibration.
"""
import argparse
import math
from pathlib import Path

import numpy as np
import soundfile as sf

from evaluate_corpus import harmonic_summary, note_name, track_harmonics


def fit_position(partials, xmin=0.04, xmax=0.30, step=0.0005):
    ns = np.array(sorted(partials), dtype=float)
    y = np.array([partials[int(n)] for n in ns], dtype=float)
    keep = np.isfinite(y) & (ns <= 12)
    ns, y = ns[keep], y[keep]
    if len(ns) < 5:
        return None

    u = np.log2(ns)
    design = np.column_stack((np.ones_like(u), u, u * u))
    best = None
    for x in np.arange(xmin, xmax + step * 0.5, step):
        # A real bichord and finite hammer width soften an exact null.  The
        # floor is only for stable log arithmetic, not a fitted noise level.
        comb = 20.0 * np.log10(np.maximum(np.abs(np.sin(np.pi * ns * x)), 0.03))
        comb -= comb[ns == 1][0]
        residual = y - comb
        beta, _, _, _ = np.linalg.lstsq(design, residual, rcond=None)
        err = residual - design @ beta
        score = float(np.sqrt(np.mean(err * err)))
        if best is None or score < best[0]:
            best = (score, float(x), len(ns), err)
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("notes", nargs="*", type=int,
                    default=(27, 35, 42, 46, 50, 53, 57, 60, 65, 72))
    ap.add_argument("--layers", nargs="+", default=("F", "MP", "PP"))
    ap.add_argument("--reference-dir", default="../GregSullivan.E-Pianos/CP80/Samples")
    args = ap.parse_args()

    root = Path(args.reference_dir)
    print("note layer window x/L rms n partials")
    for note in args.notes:
        for layer in args.layers:
            paths = sorted(root.glob(f"{note:03d}-*-{layer}.flac"))
            if not paths:
                continue
            x, sr = sf.read(paths[0], always_2d=False)
            x = np.asarray(x, dtype=float)
            if x.ndim > 1:
                x = x.mean(axis=1)
            for label, start, end in (("attack", 0.0, 0.03),
                                      ("sustain", 0.20, 0.50)):
                track = track_harmonics(x, sr, note, start, end)
                fit = fit_position(harmonic_summary(track)["partials"])
                if fit is None:
                    continue
                score, pos, count, err = fit
                print(f"{note:3d} {layer:5s} {label:7s} {pos:0.4f} "
                      f"{score:5.2f} {count:2d} "
                      f"{','.join(str(int(n)) for n in sorted(track['freqs']))}")


if __name__ == "__main__":
    main()
