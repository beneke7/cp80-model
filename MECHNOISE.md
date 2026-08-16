# Mechanical-noise experiment

Later, on a separate branch. This is an action/frame excitation experiment, not
part of the calibrated string-tone production path.

## Purpose

Test whether the CP-80's remaining attack character includes a mechanically
transmitted hammer/key-bed thump: a short, velocity-scaled event with a roughly
1 kHz key resonance and lower 100/250 Hz frame resonances. It must be routed
through the existing body/pickup path, never pasted on as output noise.

## Constraints

- Header-only, allocation-free, real-time safe; one state per voice.
- Opt-in at first; production defaults remain exactly unchanged.
- No recorded transient, broadband click, EQ, or per-note correction.
- Use the existing body and pickup filters; do not create a second output path.
- Keep string onset and mechanical onset independently measurable.

## Experiment

1. Add the smallest event state: short filtered contact component plus 100/250/1000
   Hz resonators, with independent direct and body gains.
2. Calibrate PP first by comparing noise-only and string-only RMS, then check MP/F/FF.
3. Measure attack bands, body bands, spectral flatness, crest factor, and chord
   modulation against the current corpus and demo progression.
4. Reject it if it only raises hiss/click, worsens the isolated-note score, or
   fails the chord test. Promote it only if the same physical settings improve
   both isolated attacks and assembled chords.

The later branch should also test the measured action lead separately; do not
silently add latency to the live production path.
