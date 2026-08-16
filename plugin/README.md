# CP-80 Reaper build

This is the thin JUCE wrapper around the calibrated, SDK-free engine. It builds
VST3, AU, and a standalone test app; no model code is duplicated here.

```sh
cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release
cmake --build build/plugin --config Release --parallel 8
```

The VST3 artifact is under `build/plugin/CP80_artefacts/Release/VST3/`.
Copy it to `~/Library/Audio/Plug-Ins/VST3/` or rescan plug-ins in Reaper. The
current local build has also been installed there and its AU counterpart passes
`auval`.

The panel exposes only the hardware controls: Volume, Bass, Middle, Treble,
Brilliance, Tremolo, Depth, and Speed. Tremolo produces antiphase L/R amplitude
modulation; with Tremolo off, both channels are identical.

The plugin applies a fixed 44x (+32.9 dB) line-output calibration because the
engine's raw common-voltage scale is intentionally not peak-normalized. Volume
remains the user-facing output control.
