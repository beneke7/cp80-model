// =====================================================================================
//  cp80.hpp  --  Yamaha CP-80 style electric grand, modal physical model.
//  Single header, zero dependencies, real-time safe after prepare().
//
//  MODEL
//    Per note: one lossy stiff string, simply-supported (rigid cast-iron harp,
//    no soundboard -> sin(n*pi*x/L) modes are exact, and stay decoupled).
//
//      y_n'' + 2*sigma_n*y_n' + omega_n^2*y_n = g_n * F(t)
//      omega_n = 2*pi*f0*n*sqrt(1 + B*n^2)          (stiff-string inharmonicity)
//      sigma_n = b1 + b3*omega_n^2                  (air + internal damping, the PDE form)
//      g_n     = sin(n*pi*x0/L)                     (strike position)
//
//    Hammer: point mass + power-law felt, F = K*[y_h - y(x0)]_+^p   (Hall/Askenfelt)
//    Pickup: CP-80 has 88 individual piezos on the harp casting. A piezo at a rigid
//            termination senses FORCE, so the readout is
//              F_bridge ~ sum_n (-1)^n * n * y_n
//            The +6 dB/oct that tilt implies is the instrument's actual brightness.
//
//  NUMERICS
//    Impulse-invariant discretisation of each mode:
//      y[k] = a1*y[k-1] + a2*y[k-2] + b*F[k-1]
//    The z^-1 on the input means the hammer feedback loop has a unit delay, so no
//    implicit solve is needed. Resonators are unconditionally stable by construction.
//
//  LICENCE: do what you like. Written for ToneKeep.
// =====================================================================================
#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
  #include <xmmintrin.h>
  #include <pmmintrin.h>
  #define CP80_HAS_SSE 1
#endif

namespace cp80 {

// ------------------------------------------------------------------ tunables
static constexpr int   kMaxModes    = 128;   // per voice (incl. partner resonators)
static constexpr int   kTwinBelow   = 40;    // second, phase-locked polarization; no synthetic beat
static constexpr int   kLastSingle  = 42;   // F#1: last single-strung note (Modartt/CP-80)
static constexpr int   kMaxVoices   = 24;
static constexpr int   kOversample  = 2;     // set 4 if you want tighter contact resolution
// Split between the two members of each partial pair, in cents. MEASURED off the
// reference set (see AGENTS.md §2.4): every bichord partial in the Greg Sullivan CP-80
// is a doublet, median 1.17 cents, roughly constant in cents from n=1 to n=6, i.e. the
// two strings of the unison are mistuned -- not a per-partial polarization effect.
// Single-strung notes split by polarization instead. Left at 0: only ONE clean
// measurement supports it (D#1 fundamental, 0.18 Hz -> constant in Hz, not in cents),
// and F#2 shows no beating at all, so turning it on beats notes the reference does not.
// Keep the measured range, but derive a stable note-specific interval below so a chord's
// different unisons do not all sweep the same comb.
static constexpr float kUnisonCentsMin = 0.4f;
static constexpr float kUnisonCentsMax = 2.0f;
static constexpr float kPolarHz     = 0.0f;  // single string, two transverse planes
static constexpr int   kBodyModes   = 4;    // shared cast/case resonances
static constexpr float kBodyDrive   = 0.0075f; // corpus-calibrated hammer-to-frame/piezo coupling
static constexpr float kBodyMassRef = 0.01224f; // total C4 string mass; normalizes body drive
static constexpr float kHammerRateRef = 0.71f; // MIDI velocity at the fitted forte K
static constexpr float kHammerRateQ   = 2.25f; // urethane impact-rate hardening
static constexpr float kHammerStiffnessScale = 0.70f; // softer global urethane/leather face
static constexpr float kFastPartnerWeight = 2.0f; // measured lower AM swing in the upper register
static constexpr float kMaxForce    = 400.f; // N, safety clamp on the felt nonlinearity
static constexpr int   kPruneAfter  = 4096;  // internal samples before pruning may start
static constexpr int   kWaveDelayMax = 4096; // low-register reflected-wave delay at 96 kHz
// NOTE: deferring the high modes out of the contact loop and driving them from a
// recorded force buffer was tried and is strictly SLOWER (57.8 us at 8 feedback
// modes, 39.5 us with none deferred). The mode-inner loop vectorises to full SIMD
// width; a shortened one does not. Left here so nobody re-derives it.

// =====================================================================================
//  Denormal guard. Non-optional: 100+ exponentially decaying modes will otherwise
//  drop into denormal range and cost you an order of magnitude.
// =====================================================================================
struct ScopedFTZ {
#if CP80_HAS_SSE
    unsigned oldMXCSR;
    ScopedFTZ()  { oldMXCSR = _mm_getcsr(); _mm_setcsr(oldMXCSR | 0x8040); } // FTZ|DAZ
    ~ScopedFTZ() { _mm_setcsr(oldMXCSR); }
#else
    ScopedFTZ() {} ~ScopedFTZ() {}
#endif
};

// =====================================================================================
//  Per-note voicing. This is the whole instrument: ~8 numbers, smoothly interpolated
//  from a handful of anchors. THIS IS WHERE YOU TUNE BY TASTE.
// =====================================================================================
struct NoteSpec {
    float f0;        // Hz
    float B;         // inharmonicity coefficient
    float strike;    // x0 / L
    float b1;        // flat damping   (1/s)      -> sets long decay
    float b3;        // omega^2 damping (s)       -> sets how fast the highs die
    float hammerM;   // kg
    float hammerK;   // urethane/leather stiffness scale
    float hammerP;   // contact exponent, currently global p=2
    float mass;      // total vibrating string mass (kg) -> excitation scale 2/m
    float hWidth;    // hammer contact width / L  -> spatial low-pass of the excitation
    float twinDamp;  // damping ratio of the partner resonator
    float slowW;     // output weight of the slow (long-tail) polarization
    int   nStrings;  // 1 below F#1 (midi 42), 2 above -- CP-80 is bichord, never trichord
    float unisonCents; // deterministic string-to-string mistuning for this MIDI note
    float damperRate;// extra sigma when the damper drops (1/s)
    float gain;
};

// Anchor table. midi note -> {B, strike, b1, b3, hammerM, hammerK, hammerP, damper, gain}
// The CP-80's bass strings are 67.9 cm against ~213 cm on a grand, so B is roughly an
// order of magnitude larger down there. That is the famous "wrongness" of the low end.
struct Anchor { int note; float B, strike, b1, b3, hM, hK, hP, damp, gain, mass, hW, tw, sw; };

// !! HAMMER MATERIAL IS DIFFERENT. The CP-80 spec sheet lists hammers as "Rubber (urethane)
// !! + artificial leather", not graduated felt. The production candidate therefore uses a
// !! single p=2 contact exponent, a smooth monotone K curve, and one global impact-rate
// !! hardening exponent q=2.25; it does not use per-note EQ-like hammer values. The
// !! PP/MP/F/FF gate in tools/fit_hammer.py is the calibration for that rate law.
//
// FITTED against reference CP-80 recordings (D#1 and F#2, forte) by fixed-point
// calibration: b1 <- measured slow decay, twinDamp <- fast/slow ratio, B <- measured
// inharmonicity. Hammer width is now held to a physical contact patch; it is not an
// EQ proxy. The bass string masses use the register-dependent M_H/M_s ratios from
// Chaigne & Askenfelt, which also puts the implied string tension in the piano range.
// The K anchors retain the physical bass law, harden through the A#2--C4 transition,
// then return toward the common urethane/leather facing stiffness. The b3 curve now
// rises through the upper register to match the measured early high-mode falloff.
static Anchor kAnchors[] = {
    // note   B        strike  b1     b3        hM       hK        hP    damp  gain  mass     hW      twin   slowW
    // b3 follows one register curve: the bass correction matches 3.3--3.6 kHz,
    // while the upper anchors shorten the measured 5--300 ms high-mode tail.
    {  21, 0.002065f, 0.125f, 0.225f,  4.5e-08f, 0.009f, 6.00e+08f, 2.00f, 12.0f, 1.00f, 0.099f,   0.010f, 1.25f, 0.45f },
    {  36, 0.000246f, 0.12f, 0.128f,  1.25e-08f, 0.0075f,2.77e+08f, 2.00f, 16.0f, 0.95f, 0.0533f, 0.010f, 1.25f, 0.45f },
    {  48, 0.0002389f, 0.115f, 0.128f, 1.0e-08f, 0.006f, 3.60e+08f, 2.00f, 20.0f, 0.92f, 0.0182f, 0.015f, 1.25f, 0.45f },
    {  60, 0.0002811f, 0.108f, 0.165f, 2.0e-09f, 0.0046f,2.00e+08f, 2.00f, 26.0f, 0.88f, 0.00612f, 0.020f, 2.7f, 0.45f },
    {  72, 0.0005622f, 0.095f, 0.2357f, 2.5e-09f, 0.0036f,5.41e+07f, 2.00f, 34.0f, 0.90f, 0.002635f, 0.020f,2.6f,0.45f },
    {  84, 0.002108f, 0.14f, 0.3771f, 3.5e-09f, 0.003f, 5.21e+07f, 2.00f, 44.0f, 0.95f, 0.0012f, 0.019f, 2.4f, 0.45f },
    { 108, 0.008433f, 0.16f, 0.6678f, 4.0e-09f, 0.0026f,5.00e+07f, 2.00f, 60.0f, 0.95f, 0.00045f,0.0128f,2.2f,0.45f },
};
static constexpr int kNumAnchors = int(sizeof(kAnchors) / sizeof(Anchor));

// =====================================================================================
//  Railsback stretch curve, digitised from the CP-80 spec sheet ("Tuning curve of the
//  CP-80", page 7). Key 1 = A0 = MIDI 21. Bass progressively flat, treble progressively
//  sharp, crossing zero near middle C, roughly -35 to +30 cents at the extremes.
//
//  This is a PRINTED SPECIFICATION, not tuning drift. Two independent measurements off
//  reference recordings land on it: D#1 (key 7) measured -19 cents, F#2 (key 22)
//  measured -5 cents. Those two points are anchors in the table below.
// =====================================================================================
struct StretchPoint { int key; float cents; };
static const StretchPoint kStretch[] = {
    {  1, -35.f }, {  7, -19.f },   // key 7 = D#1, measured
    { 13, -12.f }, { 22,  -5.f },   // key 22 = F#2, measured
    { 28,  -3.f }, { 40,   0.f },   // key 40 = C4
    { 52,  +2.f }, { 64,  +6.f },
    { 76, +14.f }, { 88, +30.f },
};
static constexpr int kNumStretch = int(sizeof(kStretch) / sizeof(StretchPoint));

inline float stretchCents(int note)
{
    const int key = note - 20;                     // MIDI 21 -> key 1
    if (key <= kStretch[0].key)              return kStretch[0].cents;
    if (key >= kStretch[kNumStretch-1].key)  return kStretch[kNumStretch-1].cents;
    int i = 0; while (i < kNumStretch - 2 && key > kStretch[i+1].key) ++i;
    const float t = float(key - kStretch[i].key) / float(kStretch[i+1].key - kStretch[i].key);
    return kStretch[i].cents + t * (kStretch[i+1].cents - kStretch[i].cents);
}

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float lerpLog(float a, float b, float t) { return std::exp(lerpf(std::log(a), std::log(b), t)); }
inline float unisonCentsForNote(int note)
{
    const float phase = float(note) * 0.61803398875f;
    return kUnisonCentsMin + (kUnisonCentsMax - kUnisonCentsMin) * (phase - std::floor(phase));
}

// Load anchor overrides from a text file: "index B strike b1 b3 hM hK hP damp gain
// mass hW twin slowW" per line. Lets an external fitter iterate without a rebuild.
inline void loadAnchorOverrides(const char* path)
{
    FILE* f = std::fopen(path, "r"); if (!f) return;
    int idx; Anchor a;
    while (std::fscanf(f, "%d %f %f %f %f %f %f %f %f %f %f %f %f %f", &idx,
        &a.B,&a.strike,&a.b1,&a.b3,&a.hM,&a.hK,&a.hP,&a.damp,&a.gain,&a.mass,
        &a.hW,&a.tw,&a.sw) == 14) {
        if (idx >= 0 && idx < kNumAnchors) { a.note = kAnchors[idx].note; kAnchors[idx] = a; }
    }
    std::fclose(f);
}

inline NoteSpec specForNote(int note, float tuneCents = 0.f) {
    int i = 0;
    while (i < kNumAnchors - 2 && note > kAnchors[i + 1].note) ++i;
    const Anchor& A = kAnchors[i];
    const Anchor& Bn = kAnchors[i + 1];
    float t = float(note - A.note) / float(Bn.note - A.note);
    t = std::min(1.f, std::max(0.f, t));

    NoteSpec s;
    s.f0         = 440.f * std::pow(2.f, (float(note) - 69.f
                                 + (tuneCents + stretchCents(note)) * 0.01f) / 12.f);
    s.B          = lerpLog(A.B,  Bn.B,  t);
    s.strike     = lerpf  (A.strike, Bn.strike, t);
    s.b1         = lerpf  (A.b1, Bn.b1, t);
    s.b3         = lerpLog(A.b3, Bn.b3, t);
    s.hammerM    = lerpf  (A.hM, Bn.hM, t);
    s.hammerK    = lerpLog(A.hK, Bn.hK, t);
    s.hammerP    = lerpf  (A.hP, Bn.hP, t);
    s.damperRate = lerpf  (A.damp, Bn.damp, t);
    s.gain       = lerpf  (A.gain, Bn.gain, t);
    s.mass       = lerpLog(A.mass, Bn.mass, t);
    s.hWidth     = lerpf  (A.hW,  Bn.hW,  t);
    s.twinDamp   = lerpf  (A.tw,  Bn.tw,  t);
    s.slowW      = lerpf  (A.sw,  Bn.sw,  t);
    s.nStrings   = (note <= kLastSingle) ? 1 : 2;
    s.unisonCents = unisonCentsForNote(note);
    s.mass      *= float(s.nStrings);          // hammer sees the whole unison group
    return s;
}

// =====================================================================================
//  Voice
// =====================================================================================
struct alignas(64) Voice {
    // --- mode bank, structure-of-arrays ---
    float ca1[kMaxModes], ca2[kMaxModes];   // current recursion coefficients
    float wN [kMaxModes];                   // mode radian freq, for lazy damper coeffs
    float bIn[kMaxModes];                   // impulse-invariant input gain
    float sIn[kMaxModes];                   // sin(n*pi*x0/L), hammer read-back weights
    float wOut[kMaxModes];                  // (-1)^n * n, piezo bridge-force weights
    float y1 [kMaxModes], y2[kMaxModes];    // state

    int   nModes   = 0;
    int   nActive  = 0;
    int   age      = 0;
    bool  active   = false;
    bool  held     = false;
    bool  damped   = false;
    int   note     = -1;
    uint64_t stamp = 0;

    // --- hammer ---
    bool  contact  = false;
    float hy = 0.f, hv = 0.f;
    float hK = 0.f, hP = 2.5f, hC = 0.f;
    float hFastScale = 0.f, hFastTau = 0.f, hFastR = 1.f, hFastQ = 0.f;
    float hDeltaPrev = 0.f, hMinvT = 0.f, hT = 0.f;
    float ysPrev = 0.f;          // string displacement at strike point, previous sample
    float ysPrev2 = 0.f;         // one more sample for the contact relative velocity
    int   contactAge = 0;
    float floorAmp = 1e-9f;      // prune threshold, set relative to this note's own peak
    float* contactTrace = nullptr; // optional internal-rate force trace
    int   contactTraceCapacity = 0;
    int   contactTracePos = 0;
    // Opt-in diagnostic only: a short nondispersive wave junction for contact tests.
    // The default path remains the phase-locked modal bank above.
    bool  waveContact = false;
    float waveL[kWaveDelayMax]{}, waveR[kWaveDelayMax]{};
    int   wavePos = 0, waveDelayL = 0, waveDelayR = 0;
    float waveY = 0.f, waveZ = 1.f;

    float outGain = 1.f;

    // -------------------------------------------------------------------------------
    void start(const NoteSpec& s, float velocity, double fsInternal, double fsHost, uint64_t when)
    {
        const float T = float(1.0 / fsInternal);
        // Ceiling is the OUTPUT band, not the internal one. Modes above the host
        // Nyquist are decimated away, and the string is linear so nothing folds
        // down. Generating them was pure waste -- roughly half the bank on low notes.
        const float ny = float(0.45 * fsHost);

        nModes = 0;
        const float PI = 3.14159265358979f;

        auto emit = [&](float fn, float sg, float shape, float wSign, float outW, float nIdx) {
            if (nModes >= kMaxModes) return;
            const float w = 6.2831853071795864f * fn;
            if (sg >= w) return;
            const int i = nModes;

            // sigma/omega is at most ~3e-3 anywhere in the bank (damping is either
            // b1 at low omega or b3*omega^2 at high omega, and both stay far below
            // the pole radius), so the two-term expansions below are exact in float.
            const float q2  = (sg / w) * (sg / w);
            const float wd  = w * (1.f - 0.5f * q2);
            const float rwd = (1.f / w) * (1.f + 0.5f * q2);      // 1/wd, no divide

            // exp(-sg*T) with sg*T < 3e-3  ->  cubic Taylor, error < 2e-12
            const float x = sg * T;
            const float r = 1.f - x * (1.f - x * (0.5f - x * 0.16666667f));

            const float st = std::sin(wd * T);
            const float ct = std::cos(wd * T);

            ca1 [i] = 2.f * r * ct;
            ca2 [i] = -r * r;
            wN  [i] = w;
            bIn [i] = (2.f / s.mass) * shape * T * r * st * rwd;
            sIn [i] = shape;
            wOut[i] = wSign * nIdx * outW;
            y1[i] = y2[i] = 0.f;
            ++nModes;
        };

        // sin(n*theta) for fixed theta satisfies s[n+1] = 2*cos(theta)*s[n] - s[n-1].
        // Two recurrences replace ~2*N library sin() calls with two multiply-adds
        // each. Carried in double so the error stays well below float resolution.
        const double thS = PI * double(s.strike), thW = PI * double(s.hWidth) * 0.5;
        const double c2S = 2.0 * std::cos(thS),   c2W = 2.0 * std::cos(thW);
        double sS0 = 0.0, sS1 = std::sin(thS), sW0 = 0.0, sW1 = std::sin(thW);

        for (int n = 1; n <= 256; ++n) {
            const float fn = s.f0 * float(n) * std::sqrt(1.f + s.B * float(n) * float(n));
            if (fn >= ny || nModes >= kMaxModes - 2) break;

            const float w  = 6.2831853071795864f * fn;
            const float sg = s.b1 + s.b3 * w * w;
            if (sg >= w) break;

            // strike comb x finite hammer width (spatial average -> physical HF rolloff)
            const float xw = float(thW) * float(n);
            float shape = float(sS1);
            if (xw > 1e-6f) shape *= float(sW1) / xw;

            const float sign = (n & 1) ? -1.f : 1.f;

            if (n <= kTwinBelow) {
                // Two partners per partial. The loud one couples hard to the bridge and
                // dies fast; the quiet one rings on -> two-stage decay. They are also
                // SPLIT IN FREQUENCY: measured on the reference set, every bichord
                // partial is a doublet ~1.2 cents wide (the unison mistuning), every
                // single-strung partial a narrower one (the two polarizations). The
                // split is what produces the real instrument's beating; without it the
                // pair is degenerate, the sum is one exponential, and both the beat and
                // most of the double decay disappear. Split symmetrically so the
                // perceived pitch does not move.
                const float uw = float(s.nStrings);   // bichord radiates twice
                const float dHz = 0.5f * (s.nStrings > 1 ? fn * (s.unisonCents / 1731.234f)
                                                         : kPolarHz);
                emit(fn - dHz, sg,              shape, sign, s.slowW * uw, float(n));
                emit(fn + dHz, sg * s.twinDamp, shape, sign, kFastPartnerWeight * uw, float(n));
            } else {
                // One resonator stands in for the pair above the cutoff; preserve
                // their total bridge weight instead of making the spectrum jump.
                emit(fn, sg, shape, sign, (s.slowW + kFastPartnerWeight) * float(s.nStrings), float(n));
            }
            const double tS = c2S * sS1 - sS0; sS0 = sS1; sS1 = tS;
            const double tW = c2W * sW1 - sW0; sW0 = sW1; sW1 = tW;
        }
        nActive = nModes;

        // hammer: velocity 0..1 -> 0.35 .. 5.5 m/s, mildly compressed like a real action
        const float v0 = 0.35f + 5.15f * velocity * velocity;
        contact = true;
        hy      = -1.0e-4f;      // start just under the string
        hv      =  v0;
        hK      = s.hammerK;
        hP      = s.hammerP;
        hT      = T;
        hMinvT  = T / s.hammerM;
        waveY = 0.f;
        waveZ   = 2.f * s.mass * s.f0;
        waveDelayL = int(s.strike / s.f0 * float(fsInternal) + 0.5f);
        waveDelayR = int((1.f - s.strike) / s.f0 * float(fsInternal) + 0.5f);
        if (waveDelayL < 1 || waveDelayL >= kWaveDelayMax) waveDelayL = 0;
        if (waveDelayR < 1 || waveDelayR >= kWaveDelayMax) waveDelayR = 0;
        wavePos = 0;
        std::memset(waveL, 0, sizeof(waveL));
        std::memset(waveR, 0, sizeof(waveR));
        ysPrev = 0.f;
        ysPrev2 = 0.f;
        hFastQ = 0.f;
        hDeltaPrev = 0.f;
        hFastR = 1.f;
        vb1 = s.b1; vb3 = s.b3; vDamp = s.damperRate; vT = T;
        contactAge = 0;
        floorAmp = 1e-9f;
        contactTrace = nullptr;
        contactTraceCapacity = 0;
        contactTracePos = 0;

        outGain = s.gain * 0.30f;    // overall normalisation, tune to taste
        active  = true; held = true; damped = false;
        age     = 0; stamp = when;
    }

    // Damper coefficients are built here rather than at note-on: same total work,
    // but moved off the attack, where the spike actually costs you.
    void release() {
        held = false;
        if (damped) return;
        damped = true;
        for (int i = 0; i < nActive; ++i) {
            const float w = wN[i];
            const float sg = vb1 + vb3 * w * w + vDamp;
            if (sg >= w) { ca1[i] = ca2[i] = 0.f; continue; }
            const float r = std::exp(-sg * vT);
            ca1[i] = 2.f * r * std::cos(std::sqrt(w * w - sg * sg) * vT);
            ca2[i] = -r * r;
        }
    }
    float vb1 = 0.f, vb3 = 0.f, vDamp = 0.f, vT = 0.f;

    // -------------------------------------------------------------------------------
    //  CONTACT PHASE: sample-serial, modes coupled through the felt. ~1-2 ms per note.
    //  Single fused pass: the strike-point sum for sample k+1 is accumulated while
    //  updating the modes for sample k.
    // -------------------------------------------------------------------------------
    int renderContact(float* __restrict out, int n)
    {
        const int N = nActive;
        float* __restrict A1 = ca1; float* __restrict A2 = ca2;
        float* __restrict BI = bIn; float* __restrict SI = sIn;
        float* __restrict WO = wOut;
        float* __restrict U1 = y1;  float* __restrict U2 = y2;

        int k = 0;
        for (; k < n && contact; ++k) {
            float inL = 0.f, inR = 0.f;
            const bool useWave = waveContact;
            if (useWave) {
                if (waveDelayL) { inL = waveL[wavePos]; waveL[wavePos] = 0.f; }
                if (waveDelayR) { inR = waveR[wavePos]; waveR[wavePos] = 0.f; }
            }
            const bool useFacing = hFastScale > 0.f && hFastTau > 0.f;
            const float fastK = hK * hFastScale;
            const float stringY = useWave ? waveY : ysPrev;
            const float eta = hy - stringY;
            float F = 0.f;
            float nextDelta = eta > 0.f ? eta : 0.f;
            if (!useWave) {
                if (useFacing) {
                    const float gPrev = std::pow(hDeltaPrev, hP);
                    const float g = std::pow(nextDelta, hP);
                    // A Maxwell branch: a sharp stress response to changing
                    // compression, followed by exponential facing relaxation.
                    hFastQ = hFastR * hFastQ + fastK * (g - gPrev);
                    F = eta > 0.f ? hK * g + hFastQ : 0.f;
                } else if (eta > 0.f) {
                    const float elastic = hK * std::pow(eta, hP);
                    // Hunt-Crossley-style loss: loading raises force, unloading lowers
                    // it. hC is zero in the production path until fitted against the
                    // urethane/leather attack targets.
                    const float stringV = (ysPrev - ysPrev2) / hT;
                    F = elastic * std::max(0.f, 1.f + hC * (hv - stringV));
                }
            }
            if (useWave) {
                const float gap = hy + hv * hT - waveY - 0.5f * (inL + inR) * hT;
                float Fw = 0.f;
                if (gap > 0.f) {
                    const float C = hMinvT * hT + hT / (2.f * waveZ);
                    const float prevX = useFacing ? hDeltaPrev : std::max(eta, 0.f);
                    float x = gap;
                    if (useFacing) {
                        const float gPrev = std::pow(prevX, hP);
                        const float qBase = hFastR * hFastQ - fastK * gPrev;
                        const float totalK = hK + fastK;
                        for (int it = 0; it < 6; ++it) {
                            const float xp = std::pow(x, hP);
                            const float force = totalK * xp + qBase;
                            const float dForce = totalK * hP *
                                std::pow(std::max(x, 1e-12f), hP - 1.f);
                            const float d = 1.f + C * dForce;
                            x = std::min(gap, std::max(0.f, x - (x + C * force - gap) / d));
                        }
                        const float xp = std::pow(x, hP);
                        hFastQ = qBase + fastK * xp;
                        Fw = std::max(0.f, totalK * xp + qBase);
                        nextDelta = x;
                    } else {
                        for (int it = 0; it < 6; ++it) {
                            const float xp = std::pow(x, hP);
                            const float rel = hC > 0.f ? (x - prevX) / hT : 0.f;
                            const float loss = std::max(0.f, 1.f + hC * rel);
                            const float force = hK * xp * loss;
                            const float dForce = hK *
                                (hP * std::pow(std::max(x, 1e-12f), hP - 1.f) * loss +
                                 (hC > 0.f ? xp * hC / hT : 0.f));
                            const float d = 1.f + C * dForce;
                            x = std::min(gap, std::max(0.f, x - (x + C * force - gap) / d));
                        }
                        const float rel = hC > 0.f ? (x - prevX) / hT : 0.f;
                        Fw = hK * std::pow(x, hP) * std::max(0.f, 1.f + hC * rel);
                    }
                }
                F = Fw;
            }
            if (F > kMaxForce) F = kMaxForce;
            if (contactTrace && contactTracePos < contactTraceCapacity)
                contactTrace[contactTracePos++] = F;

            hv -= F * hMinvT;
            hy += hv * hT;

            if (useWave) {
                const float vString = 0.5f * (inL + inR) + F / (2.f * waveZ);
                const float outL = vString - inL, outR = vString - inR;
                waveY += vString * hT;
                if (waveDelayL) waveL[(wavePos + waveDelayL) % kWaveDelayMax] -= outL;
                if (waveDelayR) waveR[(wavePos + waveDelayR) % kWaveDelayMax] -= outR;
                if (++wavePos == kWaveDelayMax) wavePos = 0;
            }
            if (useFacing) hDeltaPrev = nextDelta;

            // Mode-inner and fully vectorised: both reductions plus the state update
            // fold into wide SIMD. Measured strictly faster than any scheme that
            // shortens this loop -- see the note above kFeedbackModes.
            float acc = 0.f, ysNext = 0.f;
            for (int m = 0; m < N; ++m) {
                const float u = A1[m] * U1[m] + A2[m] * U2[m] + BI[m] * F;
                U2[m] = U1[m]; U1[m] = u;
                acc    += WO[m] * u;
                ysNext += SI[m] * u;
            }
            ysPrev2 = ysPrev;
            ysPrev = ysNext;
            out[k] += acc * outGain;

            if ((eta <= 0.f && hv < 0.f) || ++contactAge > 2000) {
                contact = false;
                float pk = 0.f;
                for (int m = 0; m < N; ++m) pk = std::max(pk, std::fabs(U1[m]));
                floorAmp = std::max(pk * 2e-5f, 1e-12f);
            }
        }
        age += k;
        return k;
    }

    // -------------------------------------------------------------------------------
    //  FREE PHASE: modes fully decoupled. Mode-outer / sample-inner so that the
    //  coefficients and both state words stay in registers for the whole block.
    //  4-wide unroll gives the vectoriser something obvious to chew on.
    // -------------------------------------------------------------------------------
    void renderFree(float* __restrict out, int n)
    {
        const int N = nActive;
        const float g = outGain;
        int m = 0;

        for (; m + 3 < N; m += 4) {
            float a1a = ca1[m  ], a2a = ca2[m  ], wa = wOut[m  ] * g, p1a = y1[m  ], p2a = y2[m  ];
            float a1b = ca1[m+1], a2b = ca2[m+1], wb = wOut[m+1] * g, p1b = y1[m+1], p2b = y2[m+1];
            float a1c = ca1[m+2], a2c = ca2[m+2], wc = wOut[m+2] * g, p1c = y1[m+2], p2c = y2[m+2];
            float a1d = ca1[m+3], a2d = ca2[m+3], wd = wOut[m+3] * g, p1d = y1[m+3], p2d = y2[m+3];

            for (int k = 0; k < n; ++k) {
                const float ua = a1a * p1a + a2a * p2a; p2a = p1a; p1a = ua;
                const float ub = a1b * p1b + a2b * p2b; p2b = p1b; p1b = ub;
                const float uc = a1c * p1c + a2c * p2c; p2c = p1c; p1c = uc;
                const float ud = a1d * p1d + a2d * p2d; p2d = p1d; p1d = ud;
                out[k] += wa * ua + wb * ub + wc * uc + wd * ud;
            }
            y1[m  ] = p1a; y2[m  ] = p2a;
            y1[m+1] = p1b; y2[m+1] = p2b;
            y1[m+2] = p1c; y2[m+2] = p2c;
            y1[m+3] = p1d; y2[m+3] = p2d;
        }
        for (; m < N; ++m) {
            float a1 = ca1[m], a2 = ca2[m], w = wOut[m] * g, p1 = y1[m], p2 = y2[m];
            for (int k = 0; k < n; ++k) {
                const float u = a1 * p1 + a2 * p2; p2 = p1; p1 = u;
                out[k] += w * u;
            }
            y1[m] = p1; y2[m] = p2;
        }
        age += n;
    }

    void render(float* __restrict out, int n)
    {
        int done = 0;
        if (contact) done = renderContact(out, n);
        if (done < n) renderFree(out + done, n - done);
        prune();
    }

    // sigma grows with omega^2, so the top modes always die first: shrink the bank.
    void prune()
    {
        if (age < kPruneAfter) return;
        while (nActive > 1 &&
               std::fabs(y1[nActive - 1]) < floorAmp &&
               std::fabs(y2[nActive - 1]) < floorAmp) --nActive;
        if (nActive <= 1 && std::fabs(y1[0]) < floorAmp) { active = false; nActive = 0; }
    }
};

// =====================================================================================
//  Halfband decimator (2x -> 1x). Windowed sinc, built at prepare(), FIR so it cannot
//  misbehave. Half the taps are exactly zero; we skip them.
// =====================================================================================
class Decimator2x {
public:
    // Halfband: h[i] = 0 for every even offset from centre, so 14 of the 31 taps are
    // exactly zero. Folding the remaining symmetric pairs leaves 9 multiplies per
    // output instead of 31.
    void prepare(int maxBlock)
    {
        float h[kTaps];
        for (int i = 0; i < kTaps; ++i) {
            const int   d = i - kTaps / 2;
            const float sinc = (d == 0) ? 0.5f
                             : float(std::sin(3.14159265358979 * 0.5 * d) / (3.14159265358979 * d));
            const float win  = 0.42f - 0.5f  * float(std::cos(2.0 * 3.14159265358979 * i / (kTaps - 1)))
                                     + 0.08f * float(std::cos(4.0 * 3.14159265358979 * i / (kTaps - 1)));
            h[i] = sinc * win;
        }
        float sum = 0.f; for (int i = 0; i < kTaps; ++i) sum += h[i];
        for (int i = 0; i < kTaps; ++i) h[i] /= sum;
        for (int j = 0; j < kPairs; ++j) c[j] = h[2 * j];   // folded pair coefficients
        cMid = h[kTaps / 2];
        std::memset(hist, 0, sizeof(hist));
        work.assign(size_t(kOrder) + 2 * size_t(maxBlock) + 8, 0.f);
    }

    // in: 2*n samples, out: n samples
    void process(const float* __restrict in, float* __restrict out, int n)
    {
        const int nIn = 2 * n;
        float* __restrict w = work.data();
        std::memcpy(w, hist, sizeof(float) * kOrder);
        std::memcpy(w + kOrder, in, sizeof(float) * size_t(nIn));

        for (int k = 0; k < n; ++k) {
            const float* __restrict z = w + 2 * k;
            float acc = cMid * z[kTaps / 2];
            for (int j = 0; j < kPairs; ++j) acc += c[j] * (z[2 * j] + z[kTaps - 1 - 2 * j]);
            out[k] = acc;
        }
        std::memcpy(hist, w + nIn, sizeof(float) * kOrder);
    }

private:
    static constexpr int kTaps  = 31;
    static constexpr int kOrder = kTaps - 1;
    static constexpr int kPairs = 8;
    float c[kPairs]{}, cMid = 0.f;
    float hist[kOrder]{};
    std::vector<float> work;
};

// =====================================================================================
//  Front panel. BASS / MIDDLE / TREBLE tone controls and a three-position BRILLIANCE
//  switch.
//
//  CAVEAT: the corner frequencies and Q values below are PLAUSIBLE, not transcribed.
//  The real RC network is on pages 14-15 of the CP-70B owner's manual (overall circuit
//  diagram). Replacing these with the measured values is a transcription job, not a
//  modelling one. Explicit setTone(0,0,0) is flat; prepare() leaves the tone stack flat.
// =====================================================================================
struct Biquad {
    float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    inline float process(float x){                       // transposed direct form II
        const float y = b0*x + z1; z1 = b1*x - a1*y + z2; z2 = b2*x - a2*y; return y;
    }
    void reset(){ z1=z2=0; }
    void lowShelf(float fs,float f,float dB){ shelf(fs,f,dB,true);  }
    void highShelf(float fs,float f,float dB){ shelf(fs,f,dB,false); }
    void peak(float fs,float f,float dB,float Q){
        const float A=std::pow(10.f,dB/40.f), w=6.2831853f*f/fs, al=std::sin(w)/(2.f*Q);
        const float a0=1+al/A; b0=(1+al*A)/a0; b1=(-2*std::cos(w))/a0; b2=(1-al*A)/a0;
        a1=b1; a2=(1-al/A)/a0;
    }
private:
    void shelf(float fs,float f,float dB,bool low){
        const float A=std::pow(10.f,dB/40.f), w=6.2831853f*f/fs;
        const float c=std::cos(w), sn=std::sin(w), b=std::sqrt(A)/0.9f*sn;
        float a0;
        if(low){ a0=(A+1)+(A-1)*c+b;
            b0=A*((A+1)-(A-1)*c+b)/a0; b1=2*A*((A-1)-(A+1)*c)/a0; b2=A*((A+1)-(A-1)*c-b)/a0;
            a1=-2*((A-1)+(A+1)*c)/a0;  a2=((A+1)+(A-1)*c-b)/a0;
        } else { a0=(A+1)-(A-1)*c+b;
            b0=A*((A+1)+(A-1)*c+b)/a0; b1=-2*A*((A-1)+(A+1)*c)/a0; b2=A*((A+1)+(A-1)*c-b)/a0;
            a1=2*((A-1)-(A+1)*c)/a0;   a2=((A+1)-(A-1)*c-b)/a0;
        }
    }
};

// =====================================================================================
//  Engine
// =====================================================================================
class CP80 {
public:
    void prepare(double hostSampleRate, int maxBlock)
    {
        fsHost = hostSampleRate;
        fsInt  = hostSampleRate * kOversample;
        osBuf.assign(size_t(maxBlock) * kOversample + 8, 0.f);
        deci.prepare(maxBlock);

        // pickup / preamp chain: piezo -> JFET buffer -> preamp.
        // The CP-80 puts out 78 mV max into 600 ohm; it is a low-drive, near-linear
        // chain, so this is genuinely just bandwidth, not distortion.
        setBrilliance(1);                     // MEDIUM
        setTone(0.f, -3.f, 0.f);               // measured default panel voicing
        prepareBody();
        for (auto& v : voices) v = Voice{};
        stampCounter = 0;
    }

    // lp: preamp corner (Hz). hp: AC coupling (Hz).
    void setPickupTone(float lpHz, float hpHz)
    {
        // Runs at HOST rate, after decimation: the requested corner is the pickup
        // bandwidth, and nothing
        // downstream is nonlinear, so filtering at 2x was doing twice the work for
        // no benefit. Two poles folded into one biquad so the per-sample dependency
        // chain is a single FMA instead of three stacked one-poles.
        const float k1 = std::exp(-6.2831853f * lpHz / float(fsHost));
        const float k2 = std::exp(-6.2831853f * (lpHz * 1.7f) / float(fsHost));
        lpA1 = k1 + k2;  lpA2 = -k1 * k2;  lpB0 = (1.f - k1) * (1.f - k2);
        hpA = std::exp(-6.2831853f * hpHz / float(fsHost));
    }

    // ---- front panel -------------------------------------------------------------
    void setTone(float bassDb, float midDb, float trebleDb)
    {
        bqBass.lowShelf (float(fsHost),  150.f, bassDb);
        bqMid .peak     (float(fsHost),  800.f, midDb, 0.7f);
        bqTreb.highShelf(float(fsHost), 3000.f, trebleDb);
    }
    // 0 = LOW, 1 = MEDIUM, 2 = HIGH. The CP-80 piezo buffer is wide-band; these
    // switch positions are scaled from the corpus-matched 4.4 kHz medium setting.
    void setBrilliance(int level)
    {
        static const float corner[3] = { 3300.f, 4400.f, 8000.f };
        setPickupTone(corner[level < 0 ? 0 : (level > 2 ? 2 : level)], 32.f);
    }
    void setVolume(float g){ vol = g; }

    void setSustain(bool on)
    {
        sustain = on;
        if (!on) for (auto& v : voices) if (v.active && !v.held) v.release();
    }

    void noteOn(int note, float velocity)
    {
        Voice* v = allocate();
        NoteSpec sp = specForNote(note, tuneCents);
        if (strikeOverride > 0.f && strikeOverride < 1.f) sp.strike = strikeOverride;
        if (hammerWidthOverride > 0.f) sp.hWidth = hammerWidthOverride;
        const float vHammer = 0.35f + 5.15f * velocity * velocity;
        const float vRef = 0.35f + 5.15f * kHammerRateRef * kHammerRateRef;
        sp.hammerK *= hammerScale * std::pow(vHammer / vRef, hammerRateExponent);
        if (hammerExponent > 0.f) sp.hammerP = hammerExponent;
        v->start(sp, std::min(1.f, std::max(0.f, velocity)), fsInt, fsHost, ++stampCounter);
        v->hC = hammerDamping;
        v->hFastScale = hammerFacingScale;
        v->hFastTau = hammerFacingTau;
        v->hFastR = v->hFastTau > 0.f ? std::exp(-v->hT / v->hFastTau) : 1.f;
        // The body bank is driven by hammer momentum. Its resonators are normalized
        // to unit peak response in prepareBody(), so bodyGain is an ordinary mix.
        const float bodyMassScale = sp.mass / kBodyMassRef;
        const float bodyPolarity = (note & 1) ? -1.f : 1.f;
        bodyPending += bodyPolarity * bodyGain * kBodyDrive * sp.hammerM * vHammer * bodyMassScale;
        v->note = note;
        v->waveContact = waveContact;
        v->waveZ *= waveZScale;
        v->contactTrace = traceBuffer;
        v->contactTraceCapacity = traceCapacity;
        v->contactTracePos = 0;
        traceBuffer = nullptr;
        traceCapacity = 0;
    }

    void noteOff(int note)
    {
        for (auto& v : voices)
            if (v.active && v.held && v.note == note) { v.held = false; if (!sustain) v.release(); }
    }

    void process(float* __restrict out, int n)
    {
        ScopedFTZ ftz;
        const int nOS = n * kOversample;
        std::memset(osBuf.data(), 0, sizeof(float) * size_t(nOS));

        for (auto& v : voices)
            if (v.active) { const bool wasC = v.contact; v.render(osBuf.data(), nOS);
                            if (wasC && !v.contact) lastContact = v.contactAge;
                            if (v.contactTracePos > traceSamples) traceSamples = v.contactTracePos; }

        deci.process(osBuf.data(), out, n);
        renderBody(out, n);

        // piezo/preamp bandwidth (BRILLIANCE) + AC coupling, then the tone stack at
        // the CP-80 electrical output.
        for (int k = 0; k < n; ++k) {
            const float y = lpB0 * out[k] + lpA1 * lz1 + lpA2 * lz2;
            lz2 = lz1; lz1 = y;
            hpZ = y + hpA * (hpZ - y);
            float v = y - hpZ;
            v = bqTreb.process(bqMid.process(bqBass.process(v)));
            out[k] = v * vol;
        }

    }

    void setTuneCents(float c) { tuneCents = c; }
    // global felt-stiffness multiplier: >1 = harder felt, shorter contact, brighter attack
    void setHammerScale(float g) { hammerScale = g; }
    void setHammerExponent(float p) { hammerExponent = p > 0.f ? p : 0.f; }
    void setHammerRateExponent(float q) { hammerRateExponent = q > 0.f ? q : 0.f; }
    // Optional Hunt-Crossley-style hammer loss; zero preserves the fitted baseline.
    void setHammerDamping(float c) { hammerDamping = c > 0.f ? c : 0.f; }
    // Optional fast viscoelastic facing branch; zero scale preserves the baseline.
    void setHammerFacing(float scale, float tauMs)
    {
        hammerFacingScale = scale > 0.f ? scale : 0.f;
        hammerFacingTau = tauMs > 0.f ? tauMs * 0.001f : 0.f;
    }
    void setStrike(float x) { strikeOverride = x; }
    void setHammerWidth(float x) { hammerWidthOverride = x; }
    void setWaveContact(bool on) { waveContact = on; }
    void setWaveImpedanceScale(float g) { waveZScale = g > 0.f ? g : 1.f; }
    // Shared cast/case ringing. Zero preserves the string-only model.
    void setBodyGain(float g) { bodyGain = g > 0.f ? g : 0.f; }
    // Debug only: attach a force trace to the next noteOn, at the internal sample rate.
    void setContactTrace(float* buffer, int samples)
    {
        traceBuffer = buffer; traceCapacity = samples > 0 ? samples : 0; traceSamples = 0;
    }
    int contactTraceSamples() const { return traceSamples; }

    int activeVoices() const { int c = 0; for (auto& v : voices) if (v.active) ++c; return c; }
    int activeModes()  const { int c = 0; for (auto& v : voices) if (v.active) c += v.nActive; return c; }
    // diagnostics: felt contact duration of the most recent strike, in ms
    float lastContactMs() const { return float(lastContact) * 1000.f / float(fsInt); }
    int   lastContact = 0;

private:
    void prepareBody()
    {
        static constexpr float f[kBodyModes] = { 38.f, 80.f, 170.f, 32.f };
        static constexpr float w[kBodyModes] = { 1.0f, 0.30f, 0.48f, 0.20f };
        const float T = float(1.0 / fsHost);
        for (int i = 0; i < kBodyModes; ++i) {
            // A weak, slower frame mode supplies the measured low-end sustain;
            // the three broad impact humps retain the common sigma.
            const float sigma = i == 3 ? 4.0f : 12.0f;
            const float omega = 6.2831853071795864f * f[i];
            const float wd = std::sqrt(std::max(omega * omega - sigma * sigma, 1.f));
            const float r = std::exp(-sigma * T);
            bodyA1[i] = 2.f * r * std::cos(wd * T);
            bodyA2[i] = -r * r;
            bodyW[i] = w[i];
            const float baseB = T * r * std::sin(wd * T) / wd;
            float y1 = baseB, y2 = 0.f, peak = std::fabs(y1);
            for (int k = 1; k < int(4.0f * fsHost); ++k) {
                const float y = bodyA1[i] * y1 + bodyA2[i] * y2;
                y2 = y1; y1 = y;
                peak = std::max(peak, std::fabs(y));
            }
            bodyB[i] = baseB / std::max(peak, 1e-30f);
            bodyY1[i] = bodyY2[i] = 0.f;
        }
        bodyPending = 0.f;
    }

    void renderBody(float* __restrict out, int n)
    {
        float impulse = bodyPending;
        bodyPending = 0.f;
        for (int k = 0; k < n; ++k) {
            float acc = 0.f;
            for (int i = 0; i < kBodyModes; ++i) {
                const float y = bodyA1[i] * bodyY1[i] + bodyA2[i] * bodyY2[i]
                              + bodyB[i] * impulse;
                bodyY2[i] = bodyY1[i];
                bodyY1[i] = y;
                acc += bodyW[i] * y;
            }
            out[k] += acc;
            impulse = 0.f;
        }
    }

    Voice* allocate()
    {
        for (auto& v : voices) if (!v.active) return &v;
        Voice* best = &voices[0];
        for (auto& v : voices) if (!v.held && v.stamp < best->stamp) best = &v;
        return best;
    }

    Voice   voices[kMaxVoices];
    std::vector<float> osBuf;
    Decimator2x deci;

    double fsHost = 48000.0, fsInt = 96000.0;
    float  lpA1 = 0.f, lpA2 = 0.f, lpB0 = 0.f, hpA = 0.f;
    float  lz1 = 0.f, lz2 = 0.f, hpZ = 0.f;
    float  tuneCents = 0.f, hammerScale = kHammerStiffnessScale, hammerExponent = 0.f, hammerRateExponent = kHammerRateQ, hammerDamping = 0.f;
    float  hammerFacingScale = 0.f, hammerFacingTau = 0.f;
    float  strikeOverride = 0.f, hammerWidthOverride = 0.f, vol = 1.f;
    float  bodyA1[kBodyModes]{}, bodyA2[kBodyModes]{}, bodyB[kBodyModes]{}, bodyW[kBodyModes]{};
    float  bodyY1[kBodyModes]{}, bodyY2[kBodyModes]{}, bodyPending = 0.f, bodyGain = 1.f;
    bool   waveContact = false;
    float  waveZScale = 1.f;
    float* traceBuffer = nullptr;
    int    traceCapacity = 0, traceSamples = 0;
    Biquad bqBass, bqMid, bqTreb;
    bool   sustain = false;
    uint64_t stampCounter = 0;
};

} // namespace cp80
