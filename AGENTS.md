# AGENTS.md — CP-80 modal physical model

Real-time physical model of the **Yamaha CP-70/CP-80 electric grand**. C++17,
header-only core, no dependencies.

```
make all      # demo, analyze, bench, contact_probe, pulse_probe, pulse_sweep, stress, panel_demo, verify_stretch
make demo     # renders out/demo.wav
make bench    # component profile
make check    # stability gate: MUST print 0 non-finite samples
make lib      # renders a sample library to out/model-lib (needs python3 + soundfile)
make fit_pulse # fits an empirical asymmetric force pulse through the modal bank
make fit_hybrid # fits the opt-in damped contact path against attack spectra
```

On Apple silicon: `make ARCH="-mcpu=apple-m1"`.

---

## 1. What the instrument is, and why the model is small

A CP-80 is a real grand action and real strings on a **rigid cast-iron harp with no
soundboard**, read by **88 individual piezo pickups** (one per note) mounted on the
casting. Bass strings are 67.9 cm against ~213 cm on a grand.

Every one of those is a deletion from an acoustic piano model:

| grand piano | CP-80 | consequence |
|---|---|---|
| soundboard radiator | none | no plate model, no radiation, no room |
| bridge couples all strings | rigid frame | notes are independent — no N×N coupling |
| acoustic output | electrical (piezo) | ground truth is a DI; no mic modelling |
| bridge admittance nonzero | ~rigid | simply-supported BCs are *exact*, not an approximation |

That last row is the whole architectural argument. Modal synthesis is not a shortcut
here — `sin(nπx/L)` really are the eigenfunctions.

**Tuning is stretched by specification.** The spec sheet prints a Railsback curve:
roughly −35 cents at A0, crossing zero near middle C, +30 cents at C8. This is *not*
tuning drift — it is a published spec, and two independent measurements off reference
recordings land on it (D#1 −19 cents, F#2 −5). `stretchCents()` digitises it, anchored
on those two measured points. Do not "correct" it to equal temperament.

**Hammers are rubber (urethane) + artificial leather**, not felt. See §4.1.

**Stringing:** single strings from A0 up to **F#1 (MIDI 42)**, **double** (bichord) from
G1 (MIDI 43) up. Never trichords. `kLastSingle = 42`. The timbre step at that boundary
is audible on the real instrument and is reproduced (contact time 7.14 → 4.69 ms).

---

## 2. Physics invariants — do not break these

Per mode `n` of one string:

```
ÿ_n + 2σ_n ẏ_n + ω_n² y_n = (2/m)·sin(nπx₀/L)·F(t)
ω_n = ω₁ n √(1 + B n²)          B = π³ E d⁴ / (64 T L²)
σ_n = b1 + b3 ω_n²
```

1. **Bridge readout weight is `n` exactly within a note.** The piezo sits at a rigid
   termination and senses *force*: `T ∂y/∂x|_L = (Tπ/L)Σ(−1)ⁿ n y_n`. The model keeps
   the `n` weight for the partial ladder and restores the note-level `Tπ/L` scale from
   `T/L = 4m_single f₀²`, normalized at C4. Do **not** replace `n` with `f_n/f₀`
   (= `n√(1+Bn²)`); that was a bug worth up to +7 dB at the top of the bank.

2. **Excitation gain and readout cancel.** Impulse invariance gives `b_n ∝ 1/ω_n`; the
   readout gives `w_n ∝ n`. Their product is flat. The spectral envelope must therefore
   come from the hammer pulse duration, the strike comb, and the hammer-width sinc —
   **never from a corrective EQ**. If you find yourself adding a tilt filter, something
   upstream is wrong.

3. **Impulse-invariant discretisation, not bilinear or FD.**
   `y[k] = a₁y[k−1] + a₂y[k−2] + b F[k−1]`, `a₁ = 2e^{−σT}cos(ω_d T)`, `a₂ = −e^{−2σT}`.
   Pole radius is `e^{−σT} < 1` identically → unconditionally stable, no CFL bound.
   The `z⁻¹` on the input is load-bearing: it gives the hammer loop a unit delay, so no
   implicit solve is needed. **Do not "improve" this to a direct-form biquad.**
   The default engine still follows this rule. `CP80_WAVE_CONTACT=1` is an opt-in
   junction experiment; its scalar solve is diagnostic until string dispersion and
   impedance are made frequency-dependent.

4. **Two resonators per partial below `kTwinBelow`**, one loud and fast, one quiet and
   long — and split in frequency by a deterministic note-specific cents interval on
   bichords.

   This invariant used to say the opposite ("exactly the same frequency; detune was
   omitted because it created warble not present in the Greg Sullivan target notes").
   That was wrong, and it was wrong about the reference. Re-measured 2026-08-15 on the
   Greg Sullivan set: **every double-strung note is a doublet at every partial**, median
   **1.17 cents**, roughly constant in cents from n=1 to n=6 (so the two strings of the
   unison are mistuned; it is not a per-partial polarization effect). Split in Hz per
   note: 0.16 (A#2) … 1.4 (C5). Production keeps the measured 0.4–2.0-cent range but
   hashes it by MIDI note, so different chord notes do not phase-lock the same comb;
   same-note retriggers remain deterministic.

   Degeneracy also costs the double decay. Two exponentials at the *same* frequency sum
   to something very close to one exponential: the measured fast/slow ratio was 0.9–1.6
   against 1.5–16 on the reference. Beating is what makes a two-rate decay read as one.

   Above `kTwinBelow`, the second resonator is only a CPU truncation: the single proxy
   carries the sum of both bridge weights, so the partial ladder does not acquire an
   artificial 11 dB step at a note-dependent frequency.

   `kPolarHz` (single-strung notes, the two polarizations) is left at **0**. Only D#1's
   fundamental shows a clean split there (0.18 Hz, constant in Hz not cents), F#2 shows
   none, and switching it on made F#2 beat when the reference does not. Knob, not a fit.

5. **A bichord collapses exactly onto one bank**: two strings each taking `F/2` with mass
   `m` ≡ one bank of mass `2m` under full `F`, with output weight ×2. This is exact, not
   an approximation. Don't add a second bank.

---

## 3. Ground truth — measured, and its provenance

`kAnchors` in `src/cp80.hpp` is the entire instrument. **It is the only place to tune.**

Fitted by `tools/cal.py` against mapped forte recordings from D#1 through B7; the
high-register targets remain floor-limited and are diagnostic until their windows are
reworked:

| quantity | reference | model | status |
|---|---|---|---|
| σ_slow D#1 | 0.223 | 0.222 | converged |
| σ_slow F#2 | 0.155 | 0.155 | converged |
| B F#2 | 2.49e-4 | 2.40e-4 | converged |
| tilt F#2 | −9.5 dB/oct | −8.6 | close |
| **tilt D#1** | **−11.6** | **−7.6** | **NOT converged — see §4** |

**Key finding:** `b1` from acoustic-piano literature was 2–5× too high, because on a
grand the *soundboard* is the dominant energy sink and this instrument has none. Real
F#2 loses only ~10 dB in 8 seconds. Any future parameter taken from piano literature
must be checked against this failure mode first.

**Anchors at MIDI 60 / 72 / 84 / 108 are UNCONSTRAINED.** Two reference notes pin only
the bottom three; the rest were scaled. Do not present them as measured.

### Current listening blockers

- **Upper-register modulation:** the production bichord uses a deterministic 0.4--2.0-cent
  note-specific split and a fast-partner bridge weight of 2.0. The reference also beats,
  but E5--G6 is often slower/subtler; the higher partner weight reduces the measured
  envelope swing without deleting the physical doublet.
- **Low D# growl:** reference H2--H4 sit about 13 dB above H1; the model is roughly
  11.6 dB low. Hammer stiffness and body gain do not move it. D#1's first comb null is
  near H8 (`x/L ≈ .125`), while D3's is near H5 (`x/L ≈ .20`), so strike-position
  variation remains unresolved in the bass/mid break. The upper-register comb sweep did
  support a conservative strike rise at the 84 and 108 anchors (`x/L = .14/.16`), which
  is now production; it is not a per-note correction.
- **Decay shape:** the raised b3 anchors improve the 1--4 kHz sustain, but the single
  `b1 + b3*ω²` law still over-damps parts of 4--6 kHz. This is a law-shape limitation,
  not a reason for corrective EQ.
- **Still deferred:** FF tension glide, per-note tuning scatter, exact tone-stack RC
  transcription, and the high-register damping-law fit above MIDI 84.

---

## 3b. Front panel (spec sheet, page 7)

| control | spec | status |
|---|---|---|
| Volume | — | `setVolume()` |
| Tone: BASS / MIDDLE / TREBLE | — | `setTone()` — **corner freqs are plausible, not transcribed** |
| BRILLIANCE: HIGH / MEDIUM / LOW | — | `setBrilliance(2/1/0)` → 8.0k / 4.4k / 3.3k |
| Damper pedal | — | `setSustain()` |
| PATCH OUT / IN | −20 dBm 600 Ω / 100 kΩ | downstream hardware, outside this model |
| Line out | −20 dBm 600 Ω balanced | CP-80 electrical output |

The production path uses MEDIUM brilliance with the existing tone stack at `0 / -8 / 0`
dB (bass / mid / treble), selected by a full-corpus A/B and listening pass. The tone-stack
RC values should still be transcribed from the CP-70B overall circuit diagram (owner's
manual pp. 14–15) rather than treated as an exact circuit reconstruction.

## 4. Open problems (ranked)

1. **The original bass contact was 2–3× too long** (12.77 ms at D#1; real ~4 ms).
   The strike point was following the hammer, so the old tilt fitter drove `hW` to
   0.104, a physically absurd patch. The landed correction replaces that proxy with
   Chaigne–Askenfelt register-dependent string mass and a physical bass width: D#1 is
   measured 5.35 ms and F#2 3.79 ms before the hammer-law refit. The remaining 3–6 kHz
   velocity-dependent attack deficit is a force-edge problem, not a reason to widen the hammer again. With the current global `p=2`, smooth K law, and 0.70 stiffness scale, the SFZ F-layer midpoint gives contact times of 4.99 ms at D#1, 3.51 ms at F#2 and 1.85 ms
   at C4. Finite differences during contact or a frequency-dependent wave-digital
   junction remain principled routes for the remaining velocity-dependent force edge.

   **The hammer material in the model is different.** The spec sheet lists
   "Rubber (urethane) + artificial leather", not graduated felt. The current production
   candidate uses one global `p=2`, a register law that hardens at MIDI 48/60 and then
   returns toward the common treble facing stiffness, and one global impact-rate
   hardening exponent `q=2.25`. It is
   a physical register/rate law, not per-note EQ. The PP/MP/F/FF gate now reaches roughly
   68–70 dB of the reference 72–79 dB 4–6 kHz growth; the remaining error is not hidden
   in more anchor values.

   `tools/contact_probe.cpp` records the actual internal-rate force: the current D#1 at
   F is 4.95 ms with a shortened multi-bump tail. `tools/pulse_probe.cpp` shows that a 2–4 ms prescribed pulse restores the
   missing H2–H4 cluster. The current wave-junction branch reproduces that direction
   at D#1, but a scalar nondispersive impedance rings at neighboring notes, so it is
   deliberately opt-in and not a calibration result yet.

   **Hybrid route status:** `tools/pulse_sweep.cpp` and `tools/fit_pulse.py` fit a compact
   asymmetric beta pulse before changing the contact law. `CP80_HAMMER_DAMPING` then adds
   an opt-in Hunt-Crossley-style loss term, and `tools/fit_hybrid.py` sweeps it together
   with the diagnostic wave impedance. A two-note, four-layer sweep reduced the weighted
   early-spectrum loss from 6013 to 1790, but still under-produces the D#1 H2-H4 cluster
   and over-brightens F#2 at forte. This is evidence for a remaining local bridge/modal
   mismatch, not a production calibration. The default path remains unchanged.

   **Hard-facing experiment:** `CP80_HAMMER_FACING` plus `CP80_HAMMER_TAU_MS` adds one
   Maxwell relaxation branch for the urethane/leather face. It produces the required
   sub-millisecond force edge without adding noise or EQ. A shared setting still leaves
   D#1 roughly 13 dB low in the 3--8 kHz first attack window while F#2 can match, so the
   next physical question is frequency-dependent string loading/dispersion in the wave
   junction. A 21.6 -> 43 kHz contact-mode extension was measured and removed: it did
   not change the contact trace, so it was only extra CPU.


2. **B measurement is unstable**: the same reference file reads 1.80e-4 at 2²¹ FFT /40
   partials and 5.40e-4 at 2¹⁹ /32. Needs a windowing and resolution study before B
   targets can be trusted in any fitter.

3. **Tone-stack values are invented.** Corner frequencies and Q in `setTone()` are
   plausible placeholders. Transcribe the real network from the circuit diagram.

4. **No mechanical noise — and it is not broadband, it is resonant.** Measured on the
   reference set (2026-08-15): every note from C4 to G6 carries the *same* low-frequency
   signature, broad humps near **30–45 / 70–90 / 150–190 Hz**, at −20 dB relative to the
   note's own peak in the first 50 ms, decaying with amplitude sigma ≈12. Note-independent
   frequency, so it is the casting/case ringing into the piezos, not string content; the
   humps are broad (Q ≈ 3–5), so it is not sympathetic string ringing either. The
   production `CP80_BODY_GAIN` path adds four shared low-Q resonators near
   32, 38, 80, and 170 Hz. The weak 32 Hz mode has the slower low-end decay; the other
   three use one shared fitted decay (sigma 8) to cover the useful early body envelope. It is driven by an impact-weighted hammer speed, the low-frequency limit
   of integrating the transmitted strike force, and is rendered before the common pickup
   chain. Each mode is normalized to unit peak response, the speed law uses exponent
   **1.25** (anchored at the fitted forte rate), and its scale is set by one fitted
   frame/piezo coupling, with weights **0.20 / 1.0 / 0.30 / 0.48** (32 / 38 / 80 / 170 Hz) and a default
   body mix of **1**. Its impulse is scaled by total string mass and note polarity. The reference
   flacs are peak-normalised per file, so only the relative band level and decay are used;
   no per-note body gains are introduced.

4b. **The upper damping law was underfit above ~MIDI 95.** Total-RMS decay, real vs the
   old model: MIDI 95 25.0 / 19.9 dB/s, 97 18.5 / 24.3, **102 13.7 / 40.7**. A smooth
   increase of the 72/84/108 `b3` anchors now reduces the early high-mode persistence;
   the remaining high-register windows are still floor-limited and need a dedicated
   decay estimator before further damping changes are promoted.

5. **Default contact law has no hysteresis.** Pure `K η^p` is lossless. The opt-in
   Hunt-Crossley-style term is available for route-3 sweeps, but its fitted value is not
   yet accepted as a global physical hammer constant.

6. **Bichords are lumped to 2 resonators, not 4** (2 strings × 2 polarizations). The
   string-to-string mistuning is now a deterministic 0.4–2.0 cents per MIDI note; the
   reference spread is 0.4–2.3 cents, and
   there is still no real bridge coupling — the two partners exchange no energy, they
   just sum at the pickup, so the beat depth cannot vary over the note the way
   Weinreich's coupled pair does.

---

## 5. Dead ends — measured, do not re-derive

| tried | result |
|---|---|
| Defer high modes out of the contact loop, drive from a recorded force buffer | **Strictly slower.** 57.8 µs at 8 feedback modes, 39.5 with none deferred, monotone. The mode-inner loop vectorises to full SIMD width; a shortened one does not. |
| 8-wide unroll in `renderFree` | **Slower on x86** (register spilling): 4.71 vs 3.37 µs at 420 modes. *May win on ARM* — 32 FP registers, 4 FP pipes. Re-sweep there. |
| `fastPow` via exp2(p·log2 x) with a linear mantissa approximation | **14.8% error.** Unacceptable for a force law. Reverted to `std::pow`. |
| Residual static compliance for truncated modes | No measurable effect on contact time. |
| Nelder-Mead on point-sampled decay envelopes | **Moved zero distance in 261 evaluations.** Beating nulls make the loss surface noise. Use the peak-following upper envelope in `cal.py`. |

**Optimizations that did work:** folded halfband decimator (12×, half the taps are
exactly zero); pickup filter moved to host rate as one biquad (0.937 → ~0.10 µs/blk);
mode ceiling at 0.45·fs_host not internal Nyquist; damper coefficients built lazily in
`release()`; Chebyshev recurrence for `sin(nθ)` (also **8 orders of magnitude more
accurate** than the float library call it replaced, which was passing ~43 rad arguments).

---

## 6. Measurement discipline

This project has produced **five** wrong measurements that looked plausible. Every one
was caught by cross-checking, none by reasoning. Assume the same will happen to you.

- **Keep voices alive when profiling.** An early `bench` timed 30k blocks after one
  strike; notes decayed, most blocks rendered silence, numbers were ~20× optimistic.
- **Cross-check any kernel number against `make check`.** The mode-sample cost must
  predict the stress-test core percentage to within ~10%. If it doesn't, the harness is
  wrong, not the engine.
- **Partial trackers bootstrap-fail.** Starting at B=0 and searching near harmonic
  positions locks onto noise, because at n=30 the real partial is 30% sharp. Track
  sequentially with a running B estimate (`tools/partials.py`, `tools/cal.py`).
- **Never tune in bad coordinates.** If a parameter is being driven to a physically
  absurd value to hit a target, a different parameter is wrong. Fix that one.
- **Don't change timbre during a performance pass**, or you lose track of which change
  did what.
- **A claim about the reference is a measurement, and decays like one.** §2.4 asserted
  for months that the target notes had no beating, and the model was built around that.
  They beat: 3–8 dB rms, nulls to −60 dB, on every double-strung note in the set. The
  claim was almost certainly true of *one* early experiment (equal-amplitude detuned
  twins, which warble synthetically) and got written down as a fact about the instrument.
  When a doc says "the reference does not do X", re-measure before designing around it.
- **Check the spec sheet before assuming a discrepancy is noise.** The reference tuning
  offsets were dismissed as "a piano that needed tuning"; they were a printed Railsback
  curve. Two measurements had already confirmed it and nobody looked.

Current reference numbers on a 3.2 GHz x86 box: 0.68 ns/mode-sample (bare kernel),
0.83 in-engine, noteOn ~2.0 µs, ~12× realtime on the 1689-mode stress test.
At a realistic 300-mode load that is **~1.6 µs per sample against a 20.8 µs budget**.

---

## 7. Output boundary

The model ends at the CP-80 electrical output. The real instrument puts out 78 mV max
into 600 Ω through a JFET buffer — a low-drive, essentially *linear* chain. The
nonlinearity people associate with CP-80 records is downstream: the Twin, the desk, or
the chorus. Those processors are intentionally outside this project.

---

## 8. References

- Chaigne & Askenfelt, *Numerical simulations of piano strings I & II*, JASA 95 (1994)
- Bank & Chabassier, *Model-Based Digital Pianos*, IEEE SPM 36(1), 2019 — read first
- Bank, *Physics-Based Sound Synthesis of the Piano*, MSc — loss-filter calibration,
  beating and two-stage decay measurement. Free at `home.mit.bme.hu/~bank/thesis/`.
  **Bank is at BME in Budapest.**
- Hall & Askenfelt, *Piano string excitation V*, JASA 83 (1988) — where p ∈ [2.2, 3.5]
- Askenfelt (ed.), *Five Lectures on the Acoustics of the Piano*, KTH — free
- Euphonics §12.2.1 — parameter tables, same modal+IIR architecture
- If parameters ever need *learning* rather than measuring: Zheleznov/Bilbao/Wright
  (arXiv 2601.10453), Diaz & Sandler (arXiv 2505.05940)
- CP-70B owner's manual pp. 14–15 (circuit diagram); CP-80 manual (88-piezo spec)
