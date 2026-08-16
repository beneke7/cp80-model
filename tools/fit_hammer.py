#!/usr/bin/env python3
"""Gate a global hammer exponent against available-layer attack-band growth.

The old centroid fit mixed recording tails and the missing high-band string
content into one number.  This check uses the same short, H1-relative bands as
spectral_balance.py and never edits the anchor table automatically.
"""
import argparse
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import soundfile as sf

from spectral_balance import band_levels, note_name


# Midpoints of the SFZ velocity ranges: PP 1-49, MP 50-81, F 82-103, FF 104-127.
VELS = (("PP", 25.0 / 127.0), ("MP", 65.5 / 127.0),
        ("F", 92.5 / 127.0), ("FF", 115.5 / 127.0))
DEFAULT_NOTES = (27, 42, 46, 50, 57, 60, 65, 72, 80)
P_VALUES = (1.5, 1.75, 2.0, 2.25, 2.5, 2.75, 3.0)


def compact(levels):
    """Return 0.1-1, 1-2, 2-4 and 4-6 kHz levels in dB."""
    a = [levels[("attack", lo, hi)] for lo, hi in (
        (100, 1000), (1000, 2000), (2000, 3000), (3000, 4000), (4000, 6000))]
    return np.array((a[0], a[1],
                     10.0 * np.log10(10.0 ** (a[2] / 10.0) +
                                     10.0 ** (a[3] / 10.0)), a[4]))


def load_reference(note, refdir):
    result = []
    for tag, _ in VELS:
        path = os.path.join(refdir, f"{note:03d}-{note_name(note)}-{tag}.flac")
        if not os.path.exists(path):
            continue
        x, sr = sf.read(path)
        x = np.asarray(x, dtype=float)
        if x.ndim > 1:
            x = x.mean(axis=1)
        result.append((tag, _, compact(band_levels(x, sr, note))))
    if len(result) < 2:
        raise SystemExit(f"need two velocity layers for note {note}")
    return result


def render(note, velocity, exponent):
    env = os.environ.copy()
    env["CP80_HAMMER_P"] = f"{exponent:g}"
    result = subprocess.run(["./build/analyze", str(note), str(velocity), ".30"],
                            capture_output=True, env=env, check=True)
    x = np.frombuffer(result.stdout, dtype=np.float32).astype(float)
    return compact(band_levels(x, 48000, note))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("notes", nargs="*", type=int, default=DEFAULT_NOTES)
    ap.add_argument("--reference-dir", default=os.environ.get(
        "CP80_REFERENCE_DIR", "../GregSullivan.E-Pianos/CP80/Samples"))
    args = ap.parse_args()
    print("impact-rate exponent q=", os.environ.get("CP80_HAMMER_RATE_P", "2.25"))

    references = {note: load_reference(note, args.reference_dir) for note in args.notes}
    spans = {note: (values[0][0], values[-1][0])
             for note, values in references.items()}
    target = {note: values[-1][2] - values[0][2]
              for note, values in references.items()}
    print("bands: 0.1-1, 1-2, 2-4, 4-6 kHz; values are dB relative to each file's H1")
    for note in args.notes:
        lo, hi = spans[note]
        print(f"reference {note_name(note)} {lo}->{hi}:",
              " ".join(f"{v:+.1f}" for v in target[note]))

    jobs = [(exponent, note, tag, velocity)
            for exponent in P_VALUES for note in args.notes
            for tag, velocity, _ in references[note]]
    def one(job):
        exponent, note, tag, velocity = job
        return (exponent, note, tag), render(note, velocity, exponent)
    with ThreadPoolExecutor(max_workers=min(8, len(jobs))) as pool:
        got = dict(pool.map(one, jobs))

    losses = []
    for exponent in P_VALUES:
        values = [got[(exponent, note, spans[note][1])] -
                  got[(exponent, note, spans[note][0])]
                  for note in args.notes]
        loss = float(np.mean([np.mean((value - target[note]) ** 2)
                              for value, note in zip(values, args.notes)]))
        losses.append((loss, exponent))
        print(f"p={exponent:g} loss={loss:.1f}")
        for note, value in zip(args.notes, values):
            print(f"  {note_name(note)}:", " ".join(f"{v:+.1f}" for v in value))
    loss, exponent = min(losses)
    print(f"best measured grid point: p={exponent:g}, RMS={np.sqrt(loss):.1f} dB")
    print("This is a gate, not an automatic per-note calibration.")


if __name__ == "__main__":
    main()
