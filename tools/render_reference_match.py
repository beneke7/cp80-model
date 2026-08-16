#!/usr/bin/env python3
"""Render every reference sample with per-file peak matching."""
import csv
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from math import gcd
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly


# Midpoints of the SFZ velocity ranges: PP 1-49, MP 50-81, F 82-103, FF 104-127.
VELS = {"PP": 25.0 / 127.0, "MP": 65.5 / 127.0,
        "F": 92.5 / 127.0, "FF": 115.5 / 127.0}
NAME_RE = re.compile(r"^(\d+)-(.+)-(PP|MP|F|FF)\.flac$")
SRC_SR = 48000


def load_mono(path):
    x, sr = sf.read(path, always_2d=False)
    x = np.asarray(x, dtype=np.float64)
    return (x.mean(axis=1) if x.ndim > 1 else x), sr


def render(job):
    note, tag, seconds = job
    env = os.environ.copy()
    result = subprocess.run(
        ["./build/analyze", str(note), str(VELS[tag]), f"{seconds:.6f}"],
        capture_output=True, check=True, env=env,
    )
    return note, tag, np.frombuffer(result.stdout, dtype=np.float32).astype(np.float64)


def main():
    ref_dir = Path(os.environ.get("CP80_REFERENCE_DIR", "../GregSullivan.E-Pianos/CP80/Samples"))
    out_dir = Path(sys.argv[1] if len(sys.argv) > 1 else "out/model-reference-matched")
    out_dir.mkdir(parents=True, exist_ok=True)
    refs = []
    for path in sorted(ref_dir.glob("*.flac")):
        match = NAME_RE.match(path.name)
        if not match:
            continue
        note, _, tag = match.groups()
        info = sf.info(path)
        refs.append((path, int(note), tag, info.samplerate, info.frames))
    if not refs:
        raise SystemExit(f"no reference FLACs found in {ref_dir}")

    durations = {}
    for _, note, tag, sr, frames in refs:
        durations[(note, tag)] = max(durations.get((note, tag), 0.0), frames / sr)
    jobs = [(note, tag, seconds + 0.05)
            for (note, tag), seconds in sorted(durations.items())]
    rendered = {}
    workers = min(8, os.cpu_count() or 1)
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for note, tag, audio in pool.map(render, jobs):
            rendered[(note, tag)] = audio

    report = []
    for ref_path, note, tag, ref_sr, frames in refs:
        ref, _ = load_mono(ref_path)
        model = rendered[(note, tag)]
        g = gcd(SRC_SR, ref_sr)
        model = resample_poly(model, ref_sr // g, SRC_SR // g)
        model = model[:frames]
        if len(model) < frames:
            model = np.pad(model, (0, frames - len(model)))
        ref_peak = float(np.max(np.abs(ref)))
        model_peak = float(np.max(np.abs(model)))
        gain = ref_peak / max(model_peak, 1e-30)
        matched = model * gain
        sf.write(out_dir / ref_path.name, matched.astype(np.float32), ref_sr, subtype="PCM_24")
        report.append({
            "file": ref_path.name,
            "note": note,
            "velocity": tag,
            "sample_rate": ref_sr,
            "frames": frames,
            "reference_peak": f"{ref_peak:.9g}",
            "model_peak_before": f"{model_peak:.9g}",
            "peak_gain": f"{gain:.9g}",
            "matched_peak": f"{np.max(np.abs(matched)):.9g}",
        })
    with (out_dir / "volume_match.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=report[0].keys())
        writer.writeheader()
        writer.writerows(report)
    print(f"wrote {len(report)} files to {out_dir} ({len(jobs)} renders, {workers} workers)")


if __name__ == "__main__":
    main()
