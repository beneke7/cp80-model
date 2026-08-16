# Lightweight plugin boundary

Status: SDK-free adapter implemented; JUCE VST3/AU wrapper is now in `plugin/`.
The engine remains `src/cp80.hpp` and the first Reaper build is deliberately thin.

## Product

- A single playable CP-80 instrument, with no samples and no corrective EQ.
- One `CP80` instance per plugin instance; tremolo-off output is dual-mono and
  tremolo-on output is the hardware's antiphase stereo pair.
- MIDI note-on/off and sustain pedal (CC64).
- No audio patch input, user oversampling setting, reverb, or mechanical-noise layer
  in the first release. Those would hide whether the instrument itself works.

## Adapter contract

The adapter owns only host-facing state:

```text
prepare(sampleRate, maxBlock)
for each audio block:
    split at every MIDI event sample offset (mandatory)
    apply note / pedal events
    engine.process(output, segmentLength)
```

`prepare` is the only allocation/setup phase. The audio callback must contain no
allocation, lock, file I/O, logging, or parameter discovery. Parameter changes
are converted to simple smoothed values before they reach the engine.
`process` accepts zero-length calls and safely chunks a block larger than the
declared maximum; event offsets remain exact when the host block size changes.

The final format wrapper must own the lock-free parameter snapshot. The audio
thread then passes one block-local copy to this boundary; coefficient updates
happen at most once per block. Brilliance is a real three-position panel switch
and may be crossfaded over one block to avoid a zipper.

The hardware panel defines the first public controls:

- Volume
- Bass, Middle, Treble (center detent is flat)
- Brilliance: Low / Medium / High
- Tremolo: Off / On
- Depth and Speed

Body level can remain a diagnostic/default setting for now. Hammer, strike,
wave-impedance, and damping controls stay out of the user interface until a
blind test shows that exposing one is musically useful.

## Format decision

The first format wrapper is JUCE because it reaches Reaper immediately and keeps
AU/VST3/standalone behind one small native UI. CLAP can be added later without
changing the engine or SDK-free adapter.

The engine and an offline adapter test must compile without any plugin SDK. This
keeps rendering, calibration, and regression tests independent of the wrapper.

## Acceptance checks

Before shipping a format adapter:

1. Offline adapter render matches `build/demo` for the same event stream.
2. 44.1, 48, and 96 kHz; blocks of 1, 17, 128, 2048, and oversized blocks; no non-finite samples.
3. Sample-offset MIDI events do not move when the host block size changes.
4. Sustain, repeated notes, voice stealing, and reset are deterministic.
5. Tremolo off produces identical L/R; tremolo on makes the L+R modulation cancel.
6. A dense chord remains comfortably real-time on the existing `make check`
   budget.

The plugin is ready when the user cannot reliably distinguish its output from
the canonical offline render in a blind A/B at matched level. Further spectral
changes belong in the model/evaluation loop, not in the wrapper.
