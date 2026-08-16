Put reference CP-80 recordings here. NOT tracked: third-party sample library content.
Naming:  NNN-NOTE-VEL.flac   (NNN = MIDI note, 60 = C4)

The prepared corpus in ../sample-library is useful for regression checks, but it is
not independent calibration ground truth. For calibration, use the independent Greg
Sullivan recordings already vendored alongside this project:
    CP80_REFERENCE_DIR=../GregSullivan.E-Pianos/CP80/Samples python3 tools/cal.py

tools/cal.py currently expects at minimum:
    027-D#1-F.flac      single-strung bass
    042-F#2-F.flac      last single-strung note (the bichord boundary)

Anchors at MIDI 60 / 72 / 84 / 108 are UNCONSTRAINED -- no reference data exists for
them; their values are scaled from the fitted low anchors. A forte note near C4 and one
near C6 would pin them in one calibration run.

Free sources safe to redistribute:
  - Greg Sullivan CP80 set (sullivang.net): 81 samples, 4 velocity layers
  - Pianobook "David's CP70": free, but clipped in places -- unusable for decay fits
