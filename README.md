# cp80-model

Real-time physical model of the Yamaha CP-70/CP-80 electric grand. Modal synthesis with
a nonlinear hammer-contact model and piezo bridge-force readout. Header-only core, no
dependencies, ~1% of a core for a chord.

```
make all && make demo      # -> out/demo.wav
make bench                 # component profile
make check                 # stability gate
make contact_probe         # internal hammer-force trace
make pulse_probe           # prescribed-pulse modal diagnostic
make fit_pulse             # fit an asymmetric force pulse through the modal bank
make fit_hybrid            # fit opt-in contact impedance/loss against attack spectra
CP80_REFERENCE_DIR=... .venv/bin/python tools/attack_probe.py  # attack-band diagnostic
CP80_REFERENCE_DIR=... .venv/bin/python tools/spectral_balance.py --notes 27 42
CP80_BODY_GAIN=0 .venv/bin/python tools/render_lib.py out/model-lib-body  # string-only A/B
CP80_REFERENCE_DIR=... .venv/bin/python tools/render_reference_match.py out/model-reference-matched
.venv/bin/python tools/render_reference_demo.py out/demo-reference-peakmatched.wav
CP80_DEMO_SAMPLE_DIR=out/model-reference-matched .venv/bin/python tools/render_reference_demo.py out/demo-model-peakmatched.wav
.venv/bin/python tools/evaluate_corpus.py --output out/corpus-eval
```

**Read `AGENTS.md` before changing anything.** It records the physics invariants, which
parameters are measured versus guessed, the open problems, and — importantly — five
optimizations and fitting approaches that were tried, measured, and found worse.

| file | |
|---|---|
| `src/cp80.hpp` | the whole engine; `kAnchors` is the instrument |
| `src/main.cpp` | standalone demo host |
| `tools/cal.py` | fixed-point calibration against reference recordings |
| `tools/compare.py` | model vs reference plots (partial envelope, decay) |
| `tools/partials.py` | sequential partial tracker with running B estimate |
| `tools/render_lib.py` | renders a sample library for A/B against the real thing |
| `tools/render_reference_match.py` | renders every reference file with exact duration and per-file peak matching |
| `tools/render_reference_demo.py` | replays the standalone demo score using the reference samples |
| `tools/fit_hammer.py` | gates one global contact exponent against PP/MP/F/FF band growth |
| `tools/panel_demo.cpp` | renders the same phrase through four panel settings |
| `tools/verify_stretch.cpp` | prints the applied Railsback curve vs measured anchors |
| `tools/bench.cpp` | profiler |
| `tools/stress.cpp` | 1689-mode stability + throughput gate |
| `tools/contact_probe.cpp` | internal-rate hammer-force trace and contact duration |
| `tools/pulse_probe.cpp` | compares prescribed force-pulse spectra through the modal bank |
| `tools/pulse_sweep.cpp` | evaluates beta-shaped force pulses for the hybrid hammer fit |
| `tools/fit_pulse.py` | fits pulse duration and asymmetry to reference harmonic ratios |
| `tools/fit_hybrid.py` | fits the opt-in damped contact path against reference layers |
| `tools/attack_probe.py` | separates early high-frequency attack from later decay |
| `tools/spectral_balance.py` | fixed-window band energy and sixth-order Butterworth decay check |
| `tools/evaluate_corpus.py` | multicore 81-sample scorecard with validity gates and grouped summaries |

Calibration iterates without recompiling: `analyze` reads anchor overrides from the file
named in `$CP80_ANCHORS`. Set `$CP80_REFERENCE_DIR` to use a reference corpus outside
`reference/`; use `../GregSullivan.E-Pianos/CP80/Samples` for independent calibration
and `../sample-library` only for regression checks.
For a pickup-bandwidth sweep, set `CP80_PICKUP_LP` when running `build/analyze`; the
production medium brilliance corner is 4.4 kHz, with LOW/HIGH scaled around it.
For a tone-stack sweep, set `CP80_TONE_BASS`, `CP80_TONE_MID`, and/or
`CP80_TONE_TREBLE`; these are diagnostic overrides for the existing panel EQ.
For the contact experiment, set `CP80_WAVE_CONTACT=1 CP80_WAVE_Z=<scale>` when
running `build/analyze`; it is opt-in and is not the default model path yet.
For the hybrid hammer experiment, set `CP80_HAMMER_DAMPING=<s/m>`; zero is the
unchanged baseline and the term is deliberately opt-in until its velocity sweep is fitted.
For the hard-facing experiment, add `CP80_HAMMER_FACING=<scale>
CP80_HAMMER_TAU_MS=<ms>`; it is a one-state Maxwell relaxation branch for the
urethane/leather contact, and remains opt-in until the keyboard-wide fit is physical.
The default body mix is `1`; set `CP80_BODY_GAIN=0` for the string-only model. The
body path adds four normalized shared low-Q resonators near 32, 38, 80, and 170 Hz.
The weak 32 Hz mode has the slower measured low-end decay; the weights are
`0.20 / 1.0 / 0.30 / 0.48` (32 / 38 / 80 / 170 Hz). Its impulse follows total string
mass and alternates note polarity so dense chords do not add one shared mode coherently.
For demo A/B, `CP80_BODY_GAIN` is also read by `build/demo`; the production coupling is
calibrated in `kBodyDrive`, while this control remains a reversible mix diagnostic.
The default panel voicing is the existing tone stack at `0 / -6 / 0` dB (bass / mid /
treble), selected by the corpus/ear A/B. The production hammer uses `p=2`, a global impact-rate stiffness law `q=2.25`, and a
global `0.70` stiffness scale for the softer urethane/leather facing; set
`CP80_HAMMER_RATE_P` or `CP80_HAMMER_SCALE` for diagnostic sweeps. Bichord beating keeps
the measured `0.4--2.0` cent note-specific split, with a `2.0` fast-partner bridge weight
to keep the modulation subtle.

## Conformance to the CP-80 spec sheet

Key range, 88 independent piezos, damper, output level, the printed Railsback tuning
curve, and the panel tone/brilliance controls are implemented. Two things are known-wrong: the hammer contact law
uses felt constants where the sheet says rubber + leather, and the tone-stack RC values
are guesses pending transcription from the circuit diagram.

## Status

Decay and inharmonicity are fitted to reference recordings. The bass correction uses
register-dependent string masses and a physical hammer width; the hammer candidate now
uses one global `p=2` with a smooth `hK` register curve, a 0.70 stiffness scale, and
global rate-hardening `q=2.25`. At the SFZ F-layer midpoint (`v=0.728`), contact is
4.99 ms on D#1, 3.51 ms on F#2 and 1.85 ms on C4.
The `b3` curve rises smoothly through the upper register to match the measured early
high-mode falloff;
the energy-preserving `n=40` mode
truncation removes the artificial note-dependent 11 dB ladder step. The upper-register
readout gain stays near unity so high notes do not disappear into the shared frame
component, and the strike ratio rises conservatively from 0.095 at C5 to 0.14 at C6
and 0.16 at B7 to preserve the measured upper-register comb. The fixed-window
attack check is now within roughly 2–8 dB in the 0.1–6 kHz bands on those two notes.
The PP→FF 4–6 kHz growth gate still selects the global `p=2` law; the expanded fitter
now measures the available layers from D#1 through G#5 at the SFZ layer midpoints. The remaining contact-law
mismatch is not being compensated with per-note EQ or a synthetic click.

## Corpus scorecard

The reproducible A/B loop is:

```sh
make -j8 all
.venv/bin/python tools/render_reference_match.py out/model-reference-matched
.venv/bin/python tools/evaluate_corpus.py --output out/corpus-eval
```

`evaluate_corpus.py` measures every reference/model pair using the SFZ layer midpoint and
the reference file's own H1 for relative levels. It writes:

- `metrics.csv`: one row per sample, with raw values, model-minus-reference errors, and
  `flags` for recording-floor or tracking limits;
- `summary.csv`: all samples plus PP/MP/F/FF and bass/low-mid/mid/treble aggregates;
- `worst.csv`: the same rows ordered by a diagnostic priority score;
- `definitions.json`: the windows and validity rules used for that run.

The independent observables are attack and sustain energy in 100--1000, 1--2, 2--3,
3--4, 4--6, and 6--9 kHz; body energy in 20--60, 60--120, and 120--220 Hz; early
Butterworth/Hilbert decay sigma; H1--H12 amplitudes, tilt, and inharmonicity `B`; H1
pitch and drift over 30--200 ms, 200--500 ms, and 1--2 s; unison AM depth; high-band
flatness; duration, peak/RMS, and attack-to-tail SNR. The scorecard never fits a band
whose reference attack is less than 12 dB above its tail noise, and never treats weak
partial tracking as a valid harmonic error. Contact duration, force shape, velocity
growth, and real-time stability remain separate engine diagnostics because they are
internal physical observables rather than sample waveforms.

Do not optimize the priority score directly. Use it to choose the next measurement,
then inspect the corresponding columns across the full register and velocity layer.
That keeps a physically meaningful parameter from becoming a per-note correction for
one recording artifact.
