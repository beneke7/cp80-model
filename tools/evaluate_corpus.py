#!/usr/bin/env python3
"""Measure every reference/model pair without hiding weak or noisy metrics.

The output is deliberately a table of independent observables, not one magic
loss.  Run render_reference_match.py first so model_dir contains same-duration,
per-file peak-matched renders.
"""
import argparse
import csv
import json
import math
import os
import re
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, hilbert, sosfiltfilt

from spectral_balance import BANDS, band_levels, band_sigmas, h1_peak, note_name


NAME_RE = re.compile(r"^(\d+)-(.+)-(PP|MP|F|FF)\.flac$")
BODY_BANDS = ((20, 60), (60, 120), (120, 220))
ALL_BANDS = BANDS + BODY_BANDS
LAYER_MIDPOINT = {"PP": 25.0 / 127.0, "MP": 65.5 / 127.0,
                  "F": 92.5 / 127.0, "FF": 115.5 / 127.0}
TAIL_SNR_GATE_DB = 12.0
EPS = 1e-30


def load_mono(path):
    x, sr = sf.read(path, always_2d=False)
    x = np.asarray(x, dtype=np.float64)
    return (x.mean(axis=1) if x.ndim > 1 else x), sr


def db10(x):
    return 10.0 * np.log10(max(float(x), EPS))


def db20(x):
    return 20.0 * np.log10(max(float(x), EPS))


def spectrum(x, sr, start, end, nfft=1 << 18):
    a = max(0, int(start * sr))
    b = min(len(x), int(end * sr))
    if b <= a + 32:
        return np.empty(0), np.empty(0)
    z = x[a:b]
    w = np.hanning(len(z))
    return np.fft.rfftfreq(nfft, 1.0 / sr), np.abs(np.fft.rfft(z * w, nfft))


def refined_peak(freq, mag, index):
    if index <= 0 or index >= len(mag) - 1:
        return float(freq[index]), float(mag[index])
    a, b, c = (np.log(mag[index - 1] + EPS), np.log(mag[index] + EPS),
               np.log(mag[index + 1] + EPS))
    d = np.clip(0.5 * (a - c) / (a - 2.0 * b + c + EPS), -1.0, 1.0)
    step = freq[1] - freq[0]
    return float(freq[index] + d * step), float(np.exp(b - 0.25 * (a - c) * d))


def band_power(x, sr, start, end, lo, hi):
    freq, mag = spectrum(x, sr, start, end)
    if not len(freq):
        return float("nan")
    mask = (freq >= lo) & (freq < hi)
    return float(np.sum(mag[mask] ** 2)) if np.any(mask) else float("nan")


def window_band_levels(x, sr, h1, start, end, bands):
    return {band: db10(band_power(x, sr, start, end, *band) / (h1 * h1))
            for band in bands}


def tail_snr(x, sr, start, end, lo, hi):
    duration = len(x) / sr
    tail_end = duration - 0.02
    tail_start = max(start + 0.05, duration - 0.40)
    if tail_end <= tail_start:
        return float("nan")
    signal = band_power(x, sr, start, end, lo, hi)
    noise = band_power(x, sr, tail_start, tail_end, lo, hi)
    if not np.isfinite(signal) or not np.isfinite(noise):
        return float("nan")
    return db10(signal / max(noise, EPS))


def track_harmonics(x, sr, note, start, end, f1=None, inharmonicity=None, nmax=20):
    f0 = 440.0 * 2.0 ** ((note - 69) / 12.0)
    freq, mag = spectrum(x, sr, start, end)
    if not len(freq):
        return {"f1": float("nan"), "B": float("nan"), "freqs": {}, "amps": {}}

    if f1 is None or not np.isfinite(f1):
        # H1 is close to the nominal pitch; the wider old window can select a
        # body resonance on bass notes and silently turn pitch into nonsense.
        lo, hi = np.searchsorted(freq, [0.94 * f0, 1.06 * f0])
        if hi <= lo:
            return {"f1": float("nan"), "B": float("nan"), "freqs": {}, "amps": {}}
        f1, _ = refined_peak(freq, mag, lo + int(np.argmax(mag[lo:hi])))
    B = max(0.0, float(inharmonicity or 0.0))
    limit = min(nmax, int((0.45 * sr) / max(f1, 1.0)))
    for _ in range(4 if inharmonicity is None else 1):
        freqs = {1: f1}
        amps = {}
        lo, hi = np.searchsorted(freq, [0.75 * f1, 1.25 * f1])
        if hi > lo:
            _, amps[1] = refined_peak(freq, mag, lo + int(np.argmax(mag[lo:hi])))
        for n in range(2, limit + 1):
            predicted = f1 * n * math.sqrt(1.0 + B * n * n)
            tol = max(0.012 * predicted, 0.35 * f1)
            a, b = np.searchsorted(freq, [predicted - tol, predicted + tol])
            if b <= a + 2:
                continue
            fp, ap = refined_peak(freq, mag, a + int(np.argmax(mag[a:b])))
            if ap < max(amps.get(1, EPS) * 1e-4, EPS):
                continue
            freqs[n], amps[n] = fp, ap
        if inharmonicity is not None or len(freqs) < 4:
            break
        ns = np.array(sorted(freqs), dtype=float)
        fs = np.array([freqs[int(n)] for n in ns])
        fit = np.polyfit(ns * ns, (fs / (ns * f1)) ** 2 - 1.0, 1)
        B = max(0.0, float(fit[0]))
    return {"f1": float(f1), "B": float(B), "freqs": freqs, "amps": amps}


def harmonic_summary(track):
    amps = track["amps"]
    if 1 not in amps or len(amps) < 3:
        return {"tilt": float("nan"), "partials": {}}
    ns = np.array(sorted(amps), dtype=float)
    aa = np.array([amps[int(n)] for n in ns])
    keep = aa > aa[0] * 10.0 ** (-70.0 / 20.0)
    ns, aa = ns[keep], aa[keep]
    if len(ns) < 3:
        tilt = float("nan")
    else:
        tilt = float(np.polyfit(np.log2(ns), 20.0 * np.log10(np.maximum(aa / aa[0], EPS)), 1)[0])
    return {"tilt": tilt,
            "partials": {int(n): db20(amps[int(n)] / amps[1]) for n in amps}}


def amplitude_modulation(x, sr, frequency, duration):
    if not np.isfinite(frequency) or duration < 0.50:
        return float("nan")
    start, end = 0.15, min(1.0, duration - 0.03)
    if end <= start + 0.10 or frequency * 0.985 < 10.0:
        return float("nan")
    lo, hi = frequency * 0.985, min(sr * 0.48, frequency * 1.015)
    try:
        z = sosfiltfilt(butter(6, (lo, hi), btype="bandpass", fs=sr, output="sos"), x)
    except ValueError:
        return float("nan")
    env = np.abs(hilbert(z))[int(start * sr):int(end * sr)]
    if len(env) < 64 or np.max(env) <= 0.0:
        return float("nan")
    t = np.arange(len(env), dtype=float) / sr
    edb = 20.0 * np.log10(np.maximum(env, EPS))
    trend = np.polyval(np.polyfit(t, edb, 1), t)
    return float(np.std(edb - trend))


def signal_metrics(x, sr, note):
    duration = len(x) / sr
    h1 = h1_peak(x, sr, note)
    levels = band_levels(x, sr, note)
    body = {}
    for label, start, end in (("attack", 0.0, 0.03), ("sustain", 0.20, 0.50)):
        if end < duration:
            body[label] = window_band_levels(x, sr, h1, start, end, BODY_BANDS)
        else:
            body[label] = {b: float("nan") for b in BODY_BANDS}
    sigmas = band_sigmas(x, sr)

    attack = track_harmonics(x, sr, note, 0.0, min(0.03, duration))
    # Re-find H1 in each window so FF tension glide is measurable. Keep B
    # fixed for harmonic tracking; pitch gets its own stable early window.
    pitch_early = track_harmonics(x, sr, note, 0.03, min(0.20, duration),
                                  None, None)
    sustain = track_harmonics(x, sr, note, 0.20, min(0.50, duration),
                               None, attack["B"])
    late = track_harmonics(x, sr, note, 1.0, min(2.0, duration),
                           None, attack["B"])
    ha, hs, hl = (harmonic_summary(attack), harmonic_summary(sustain), harmonic_summary(late))
    am = {}
    for n in range(1, 7):
        # AM is measured during sustain; use the sustain track, not a
        # transient peak that may be shifted by the hammer/body attack.
        predicted = sustain["freqs"].get(n, float("nan"))
        am[n] = amplitude_modulation(x, sr, predicted, duration)

    snr = {band: tail_snr(x, sr, 0.0, min(0.03, duration), *band)
           for band in ALL_BANDS}
    return {
        "duration": duration, "peak": float(np.max(np.abs(x))),
        "rms": float(np.sqrt(np.mean(x * x))), "h1": h1,
        "levels": levels, "body": body, "sigmas": sigmas,
        "harmonic": {"attack": ha, "sustain": hs, "late": hl,
                      "f1": attack["f1"],
                      "f1_windows": {"attack": attack["f1"],
                                     "sustain": sustain["f1"],
                                     "late": late["f1"]},
                      "B": attack["B"]},
        "pitch": {"f1_windows": {"early": pitch_early["f1"],
                                   "sustain": sustain["f1"],
                                   "late": late["f1"]},
                  "partials": {"early": len(pitch_early["freqs"]),
                               "sustain": len(sustain["freqs"]),
                               "late": len(late["freqs"])}},
        "am": am, "snr": snr,
    }


def sfz_metadata(path):
    if not path.exists():
        return {}
    current = {}
    result = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if line.startswith("lovel="):
            current["lovel"] = int(line.split("=", 1)[1])
        elif line.startswith("hivel="):
            current["hivel"] = int(line.split("=", 1)[1])
        elif line.startswith("group_label="):
            current["layer"] = line.split("=", 1)[1]
        elif line.startswith("<region>"):
            match = re.search(
                r"lokey=(\d+) hikey=(\d+).*?volume=([-+]?\d+).*?sample=([^ ]+)", line)
            if not match:
                continue
            lokey, hikey, volume, sample = match.groups()
            sample = sample.replace(".$EXT", ".flac")
            result[sample] = {
                **current, "lokey": int(lokey), "hikey": int(hikey),
                "volume_db": int(volume),
            }
    return result


def pair_row(ref_path, model_path, sfz, volume):
    match = NAME_RE.match(ref_path.name)
    if not match:
        return None
    note, _, tag = match.groups()
    note = int(note)
    ref, ref_sr = load_mono(ref_path)
    model, model_sr = load_mono(model_path)
    r = signal_metrics(ref, ref_sr, note)
    m = signal_metrics(model, model_sr, note)
    row = {
        "file": ref_path.name, "note": note, "note_name": note_name(note), "velocity": tag,
        "layer_midpoint": LAYER_MIDPOINT[tag], "sample_rate": ref_sr,
        "duration_s": r["duration"], "model_duration_s": m["duration"],
        "sfz_volume_db": sfz.get(ref_path.name, {}).get("volume_db", ""),
        "sfz_lokey": sfz.get(ref_path.name, {}).get("lokey", ""),
        "sfz_hikey": sfz.get(ref_path.name, {}).get("hikey", ""),
        "ref_peak": r["peak"], "model_peak": m["peak"],
        "ref_rms": r["rms"], "model_rms": m["rms"],
        "peak_delta_db": db20(m["peak"] / max(r["peak"], EPS)),
        "rms_delta_db": db20(m["rms"] / max(r["rms"], EPS)),
    }
    if volume:
        info = volume.get(ref_path.name, {})
        row.update({"model_peak_before": info.get("model_peak_before", ""),
                    "peak_match_gain": info.get("peak_gain", ""),
                    "peak_match_gain_db": db20(float(info["peak_gain"]))
                    if info.get("peak_gain") else ""})

    attack_errors, sustain_errors = [], []
    body_attack_errors, body_sustain_errors = [], []
    for label, bands in (("attack", BANDS), ("sustain", BANDS)):
        for lo, hi in bands:
            key = (label, lo, hi)
            a, b = r["levels"][key], m["levels"][key]
            stem = f"{label}_{lo:g}_{hi:g}"
            row[f"{stem}_ref_db"], row[f"{stem}_model_db"] = a, b
            row[f"{stem}_err_db"] = b - a
            if np.isfinite(a) and np.isfinite(b):
                (attack_errors if label == "attack" else sustain_errors).append(b - a)
    for label in ("attack", "sustain"):
        for lo, hi in BODY_BANDS:
            key = (lo, hi)
            a, b = r["body"][label][key], m["body"][label][key]
            stem = f"body_{label}_{lo:g}_{hi:g}"
            row[f"{stem}_ref_db"], row[f"{stem}_model_db"] = a, b
            row[f"{stem}_err_db"] = b - a
            if np.isfinite(a) and np.isfinite(b):
                (body_attack_errors if label == "attack" else body_sustain_errors).append(b - a)

    for i, (lo, hi) in enumerate(BANDS):
        a, b = r["sigmas"][i], m["sigmas"][i]
        row[f"sigma_{lo:g}_{hi:g}_ref_s"], row[f"sigma_{lo:g}_{hi:g}_model_s"] = a, b
        row[f"sigma_{lo:g}_{hi:g}_err_s"] = b - a if np.isfinite(a) and np.isfinite(b) else float("nan")

    for name, a, b in (("f1_hz", r["harmonic"]["f1"], m["harmonic"]["f1"]),
                       ("B", r["harmonic"]["B"], m["harmonic"]["B"]),
                       ("tilt_attack_db_oct", r["harmonic"]["attack"]["tilt"], m["harmonic"]["attack"]["tilt"]),
                       ("tilt_sustain_db_oct", r["harmonic"]["sustain"]["tilt"], m["harmonic"]["sustain"]["tilt"])):
        row[f"{name}_ref"], row[f"{name}_model"] = a, b
        row[f"{name}_err"] = b - a if np.isfinite(a) and np.isfinite(b) else float("nan")
    nominal_f0 = 440.0 * 2.0 ** ((note - 69) / 12.0)
    for window in ("early", "sustain", "late"):
        a = r["pitch"]["f1_windows"][window]
        b = m["pitch"]["f1_windows"][window]
        stem = f"pitch_f1_{window}_hz"
        row[f"{stem}_ref"], row[f"{stem}_model"] = a, b
        row[f"{stem}_err"] = b - a if np.isfinite(a) and np.isfinite(b) else float("nan")
        for source, value in (("ref", a), ("model", b)):
            row[f"pitch_{window}_cents_{source}"] = (
                1200.0 * math.log2(value / nominal_f0)
                if np.isfinite(value) and value > 0.0 else float("nan"))
    for window in ("sustain", "late"):
        a = r["pitch"]["f1_windows"][window] - r["pitch"]["f1_windows"]["early"]
        b = m["pitch"]["f1_windows"][window] - m["pitch"]["f1_windows"]["early"]
        row[f"pitch_{window}_minus_early_hz_ref"] = a
        row[f"pitch_{window}_minus_early_hz_model"] = b
        row[f"pitch_{window}_minus_early_hz_err"] = b - a
        for source, attack_f1, window_f1 in (
                ("ref", r["pitch"]["f1_windows"]["early"],
                 r["pitch"]["f1_windows"][window]),
                ("model", m["pitch"]["f1_windows"]["early"],
                 m["pitch"]["f1_windows"][window])):
            drift = (1200.0 * math.log2(window_f1 / attack_f1)
                     if np.isfinite(attack_f1) and np.isfinite(window_f1)
                     and attack_f1 > 0.0 and window_f1 > 0.0 else float("nan"))
            row[f"pitch_{window}_drift_cents_{source}"] = drift
        row[f"pitch_{window}_drift_cents_err"] = (
            row[f"pitch_{window}_drift_cents_model"]
            - row[f"pitch_{window}_drift_cents_ref"]
            if np.isfinite(row[f"pitch_{window}_drift_cents_model"])
            and np.isfinite(row[f"pitch_{window}_drift_cents_ref"]) else float("nan"))
    for window in ("attack", "sustain"):
        diffs = []
        for n in range(1, 13):
            a = r["harmonic"][window]["partials"].get(n, float("nan"))
            b = m["harmonic"][window]["partials"].get(n, float("nan"))
            row[f"h{n}_{window}_ref_db"], row[f"h{n}_{window}_model_db"] = a, b
            row[f"h{n}_{window}_err_db"] = b - a if np.isfinite(a) and np.isfinite(b) else float("nan")
            if np.isfinite(a) and np.isfinite(b):
                diffs.append(b - a)
        row[f"harmonic_{window}_rmse_db"] = float(np.sqrt(np.mean(np.square(diffs)))) if diffs else float("nan")
        row[f"harmonic_{window}_bias_db"] = float(np.mean(diffs)) if diffs else float("nan")
    for n in range(1, 7):
        a, b = r["am"][n], m["am"][n]
        row[f"am_h{n}_ref_db_rms"], row[f"am_h{n}_model_db_rms"] = a, b
        row[f"am_h{n}_err_db_rms"] = b - a if np.isfinite(a) and np.isfinite(b) else float("nan")
    am_diffs = [row[f"am_h{n}_err_db_rms"] for n in range(1, 7)
                if np.isfinite(row[f"am_h{n}_err_db_rms"])]
    row["am_rmse_db_rms"] = float(np.sqrt(np.mean(np.square(am_diffs)))) if am_diffs else float("nan")
    for lo, hi in ALL_BANDS:
        key = f"{lo:g}_{hi:g}"
        row[f"snr_{key}_ref_db"], row[f"snr_{key}_model_db"] = r["snr"][(lo, hi)], m["snr"][(lo, hi)]

    flags = []
    if m["duration"] + 1e-6 < r["duration"]:
        flags.append("model_short")
    for lo, hi in ALL_BANDS:
        a = r["snr"][(lo, hi)]
        if np.isfinite(a) and a < TAIL_SNR_GATE_DB:
            flags.append(f"floor_{lo:g}_{hi:g}")
    if len(r["harmonic"]["attack"]["partials"]) < 4:
        flags.append("few_attack_partials")
    pitch_valid = min(r["pitch"]["partials"]["early"],
                      r["pitch"]["partials"]["sustain"],
                      m["pitch"]["partials"]["early"],
                      m["pitch"]["partials"]["sustain"]) >= 3
    if not pitch_valid:
        flags.append("few_pitch_partials")
    row["flags"] = ";".join(flags)
    def rms(values):
        values = [value for value in values if np.isfinite(value)]
        return float(np.sqrt(np.mean(np.square(values)))) if values else float("nan")

    valid_attack = [row[f"attack_{lo:g}_{hi:g}_err_db"]
                    for lo, hi in BANDS
                    if r["snr"][(lo, hi)] >= TAIL_SNR_GATE_DB]
    valid_sustain = [row[f"sustain_{lo:g}_{hi:g}_err_db"]
                     for lo, hi in BANDS
                     if r["snr"][(lo, hi)] >= TAIL_SNR_GATE_DB]
    valid_body_attack = [row[f"body_attack_{lo:g}_{hi:g}_err_db"]
                         for lo, hi in BODY_BANDS
                         if r["snr"][(lo, hi)] >= TAIL_SNR_GATE_DB]
    valid_body_sustain = [row[f"body_sustain_{lo:g}_{hi:g}_err_db"]
                          for lo, hi in BODY_BANDS
                          if r["snr"][(lo, hi)] >= TAIL_SNR_GATE_DB]
    row["valid_attack_bands"], row["valid_sustain_bands"] = len(valid_attack), len(valid_sustain)
    row["valid_body_attack_bands"], row["valid_body_sustain_bands"] = (
        len(valid_body_attack), len(valid_body_sustain))
    row["attack_band_rmse_db"] = rms(attack_errors)
    row["sustain_band_rmse_db"] = rms(sustain_errors)
    row["body_attack_rmse_db"] = rms(body_attack_errors)
    row["body_sustain_rmse_db"] = rms(body_sustain_errors)
    row["attack_band_rmse_valid_db"] = rms(valid_attack)
    row["sustain_band_rmse_valid_db"] = rms(valid_sustain)
    row["body_attack_rmse_valid_db"] = rms(valid_body_attack)
    row["body_sustain_rmse_valid_db"] = rms(valid_body_sustain)
    enough_attack = len(r["harmonic"]["attack"]["partials"]) >= 4
    for window in ("attack", "sustain"):
        count = min(len(r["harmonic"][window]["partials"]),
                    len(m["harmonic"][window]["partials"]))
        row[f"valid_{window}_harmonics"] = count if enough_attack and count >= 3 else 0
        row[f"harmonic_{window}_rmse_valid_db"] = (
            row[f"harmonic_{window}_rmse_db"]
            if row[f"valid_{window}_harmonics"] else float("nan"))
    row["valid_pitch_windows"] = 3 if pitch_valid else 0
    for window in ("sustain", "late"):
        row[f"pitch_{window}_drift_cents_valid_err"] = (
            row[f"pitch_{window}_drift_cents_err"] if pitch_valid else float("nan"))
    priority_values = [row["attack_band_rmse_valid_db"], row["sustain_band_rmse_valid_db"],
                       row["body_attack_rmse_valid_db"], row["body_sustain_rmse_valid_db"],
                       row["harmonic_attack_rmse_valid_db"],
                       row["harmonic_sustain_rmse_valid_db"]]
    row["priority_score_db"] = rms(priority_values)
    return row


def write_csv(path, rows):
    fields = sorted({key for row in rows for key in row})
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def aggregate(rows, label, selected):
    def mean(key):
        values = [float(row[key]) for row in selected
                  if row.get(key, "") != "" and np.isfinite(float(row[key]))]
        return float(np.mean(values)) if values else float("nan")
    return {
        "group": label, "files": len(selected),
        "attack_band_rmse_db": mean("attack_band_rmse_db"),
        "sustain_band_rmse_db": mean("sustain_band_rmse_db"),
        "attack_band_rmse_valid_db": mean("attack_band_rmse_valid_db"),
        "sustain_band_rmse_valid_db": mean("sustain_band_rmse_valid_db"),
        "body_attack_rmse_valid_db": mean("body_attack_rmse_valid_db"),
        "body_sustain_rmse_valid_db": mean("body_sustain_rmse_valid_db"),
        "harmonic_attack_rmse_db": mean("harmonic_attack_rmse_db"),
        "harmonic_sustain_rmse_db": mean("harmonic_sustain_rmse_db"),
        "harmonic_attack_rmse_valid_db": mean("harmonic_attack_rmse_valid_db"),
        "harmonic_sustain_rmse_valid_db": mean("harmonic_sustain_rmse_valid_db"),
        "pitch_sustain_drift_cents_valid_err": mean("pitch_sustain_drift_cents_valid_err"),
        "pitch_late_drift_cents_valid_err": mean("pitch_late_drift_cents_valid_err"),
        "am_rmse_db_rms": mean("am_rmse_db_rms"),
        "priority_score_db": mean("priority_score_db"),
        "peak_delta_db": mean("peak_delta_db"), "rms_delta_db": mean("rms_delta_db"),
    }


def priority(row):
    try:
        value = float(row.get("priority_score_db", "nan"))
    except (TypeError, ValueError):
        return -1.0
    return value if np.isfinite(value) else -1.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--reference-dir", default=os.environ.get(
        "CP80_REFERENCE_DIR", "../GregSullivan.E-Pianos/CP80/Samples"))
    ap.add_argument("--model-dir", default=os.environ.get("CP80_MODEL_DIR", "out/model-reference-matched"))
    ap.add_argument("--output", default="out/corpus-eval")
    ap.add_argument("--workers", type=int, default=min(8, os.cpu_count() or 1))
    args = ap.parse_args()
    ref_dir, model_dir, out_dir = Path(args.reference_dir), Path(args.model_dir), Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)
    sfz = sfz_metadata(ref_dir.parent / "CP80.sfz")
    volume = {}
    volume_csv = model_dir / "volume_match.csv"
    if volume_csv.exists():
        with volume_csv.open(newline="") as f:
            volume = {row["file"]: row for row in csv.DictReader(f)}
    pairs = []
    for ref_path in sorted(ref_dir.glob("*.flac")):
        if NAME_RE.match(ref_path.name):
            model_path = model_dir / ref_path.name
            pairs.append((ref_path, model_path))
    if not pairs:
        raise SystemExit(f"no reference files found in {ref_dir}")
    def run(pair):
        ref_path, model_path = pair
        if not model_path.exists():
            return {"file": ref_path.name, "flags": "missing_model"}
        return pair_row(ref_path, model_path, sfz, volume)
    with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
        rows = [row for row in pool.map(run, pairs) if row]
    write_csv(out_dir / "metrics.csv", rows)
    write_csv(out_dir / "worst.csv", sorted(rows, key=priority, reverse=True))
    groups = [("all", rows)]
    for tag in ("PP", "MP", "F", "FF"):
        groups.append((f"layer:{tag}", [r for r in rows if r.get("velocity") == tag]))
    for label, lo, hi in (("bass", 21, 45), ("low_mid", 46, 60),
                          ("mid", 61, 76), ("treble", 77, 108)):
        groups.append((f"register:{label}", [r for r in rows if lo <= int(r.get("note", -1)) <= hi]))
    summary = [aggregate(rows, label, selected) for label, selected in groups]
    write_csv(out_dir / "summary.csv", summary)
    definitions = {
        "attack": "0-30 ms; all dB bands relative to the attack H1",
        "sustain": "200-500 ms; same attack H1 reference",
        "sigma": "Butterworth/Hilbert amplitude decay, 50-300 ms, 45 dB relative floor",
        "harmonics": "FFT-tracked stiff-string partials H1-H12; tilt in dB/octave",
        "pitch": "H1 frequency in 30-200 ms, 200-500 ms, and 1-2 s windows; cents from nominal and drift from the stable early window",
        "am": "detrended Hilbert-envelope RMS in dB over 150 ms-1 s",
        "tail_gate": f"reference attack-to-tail SNR below {TAIL_SNR_GATE_DB:g} dB is flagged, not fitted",
        "body": "20-60, 60-120, 120-220 Hz relative to attack H1",
        "raw_vs_valid": "raw RMSE includes every finite value; *_valid_db and priority exclude reference bands below the tail gate and weak harmonic tracks",
        "priority_score": "RMS of valid attack/sustain/body band and harmonic-ladder errors; diagnostic ordering only",
    }
    (out_dir / "definitions.json").write_text(json.dumps({
        "files": len(rows), "valid_model_pairs": sum("missing_model" not in r.get("flags", "") for r in rows),
        "definitions": definitions,
    }, indent=2) + "\n")
    print(f"wrote {len(rows)} rows to {out_dir} using {max(1, args.workers)} workers")
    print(f"summary: {out_dir / 'summary.csv'}")
    print(f"worst:   {out_dir / 'worst.csv'}")


if __name__ == "__main__":
    main()
