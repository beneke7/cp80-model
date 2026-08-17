# CP-80 model: mathematical and calibration specification

This is the “lazy physics professor” account of the model as it exists in the
repository. It is intentionally more exact about assumptions, estimators, and
failure modes than a normal README. The source of truth is still
src/cp80.hpp; this file explains what each important group of lines means and
what the calibration numbers can—and cannot—prove.

The model is a real-time, modal, force-readout model of a CP-70/CP-80 electric
grand: strings, hammer contact, a rigid cast-iron frame, piezo pickups, and a
small electrical output chain. It is not a model of a wooden soundboard, air
radiation, microphones, room acoustics, or a downstream amplifier.

## 1. The three kinds of truth

Almost every calibration argument in this project becomes clear if three
different objects are kept separate.

1. **Physical observables** are quantities the model claims to mean something in
   the instrument: string length, mass, tension, strike position, contact time,
   hammer stiffness, modal damping, bridge force, and frame resonance.
2. **Waveform observables** are measurements made on a recording: band energy,
   partial amplitude, inharmonicity, decay rate, pitch drift, and beating.
3. **Optimization proxies** are numerical summaries used to decide which trial
   is better: an RMS of dB residuals, a weighted H2–H8 loss, or a chord-band
   delta.

An optimization proxy is not automatically a physical target. In particular,
per-file peak matching is useful for spectral shape but destroys absolute
register level; a recording tail can be noise rather than instrument energy;
and a one-note score can be good while a shared frame mode cancels or explodes
in a chord. The project therefore treats the scorecard as a diagnostic map,
not as one sacred scalar objective.

## 2. Notation and units

| symbol | meaning | units |
|---|---|---|
| x, L | longitudinal coordinate and vibrating string length | m |
| t, T | time and one internal sample interval | s |
| m, m_h | total vibrating string mass and hammer mass | kg |
| μ=m/L | linear string density | kg/m |
| T_s | string tension; called tension here to avoid confusing it with sample period | N |
| f_0, ω_1 | fundamental frequency and angular fundamental | Hz, rad/s |
| n | partial/mode index | dimensionless |
| B | stiff-string inharmonicity coefficient | dimensionless |
| σ_n | amplitude-decay constant of mode n | 1/s |
| q_n or u_n | modal displacement/state | model-dependent displacement |
| F | hammer force on the string | N |
| x_0/L | normalized strike point | dimensionless |
| hW | normalized hammer contact width | dimensionless |
| p | compression exponent of the contact law | dimensionless |
| K | contact stiffness scale | model-dependent |
| A | amplitude; P=A² is proportional to power | arbitrary signal units |

The code uses float state but computes some modal spatial recurrences in double.
The calibration scripts generally convert audio to float64 before FFT and
regression. This is enough precision for the present measurements; it does not
make the recording itself exact.

## 3. Why the model can be small

The physical deletion is unusually generous. A normal acoustic piano needs a
bridge, soundboard, radiation, room, and often a model of many coupled strings.
The CP-80 has a rigid cast-iron harp, no soundboard, and one piezo per note.
The intended approximation is therefore:

- each note is an independent string bank;
- the string ends are close to rigid, so simply-supported spatial modes are a
  useful first basis;
- the pickup measures local mechanical force at a termination, not a radiated
  pressure field;
- a shared low-frequency frame bank is sufficient for the measured body
  signature, at least as a first-order approximation.

The independence is not a claim that the iron frame is infinitely rigid. It is
the architectural reason an N-note chord does not require an N-by-N
string-coupling matrix. The frame is reintroduced as four shared low-Q modes,
not as a full plate finite-element model.

## 4. Continuous string model

### 4.1 Spatial basis

The source comment at src/cp80.hpp:5–18 starts from a transverse displacement
expanded in sine modes:

$$$$
y(x,t)=\sum_{n=1}^{\infty}q_n(t)\,\phi_n(x),
\qquad
\phi_n(x)=\sin\!\left(\frac{n\pi x}{L}\right).
$$$$

For ideal simply-supported ends, φ_n(0)=φ_n(L)=0. The basis is not chosen
merely because it is convenient: it is the eigenbasis of the idealized
string boundary-value problem. In a real CP-80, bridge compliance, termination
geometry, and castings perturb this basis. The current model puts those effects
into fitted damping, strike parameters, a readout scale, and an opt-in wave
junction rather than replacing the basis.

### 4.2 Modal forcing by a point strike

If a force F(t) is applied at x_0, projecting the point force onto a sine
mode gives a factor φ_n(x_0). With the normalization used by the source,
the modal equation is:

$$$$
\ddot q_n(t)+2\sigma_n\dot q_n(t)+\omega_n^2q_n(t)
=\frac{2}{m}\sin\!\left(\frac{n\pi x_0}{L}\right)F(t).
$$$$

The factor 2/m is the modal normalization for a string whose sine mode has
half the total modal mass. In code, s.mass is the whole vibrating unison mass
seen by the hammer, while each emitted mode receives 2/s.mass in
Voice::start() at src/cp80.hpp:297–323.

The spatial excitation factor is not just the point-strike comb. A hammer has a
finite contact patch. Averaging the sine shape over a centered patch of width
w gives:

$$$$
\frac{1}{w}\int_{x_0-w/2}^{x_0+w/2}
\sin\!\left(\frac{n\pi x}{L}\right)\,dx
=
\sin\!\left(\frac{n\pi x_0}{L}\right)
\operatorname{sinc}\!\left(\frac{n\pi w}{2L}\right),
$$$$

where sinc(z)=sin(z)/z. The code stores hWidth=w/L, so lines
330–345 construct:

$$$$
\theta_S=\pi\frac{x_0}{L},
\qquad
\theta_W=\frac{\pi}{2}\frac{w}{L},
\qquad
g_n=\sin(n\theta_S)\frac{\sin(n\theta_W)}{n\theta_W}.
$$$$

This is why the old hW=0.104 was disastrous in the bass: the sinc zero is
reached at a low partial index. It was not a harmless “brightness” parameter;
it represented a very large contact patch and therefore deleted high spatial
harmonics at the source.

### 4.3 Stiff-string frequencies

The current dispersion law is the standard low-order stiff-string form:

$$$$
\omega_n=2\pi f_0\,n\sqrt{1+B n^2},
\qquad
f_n=f_0\,n\sqrt{1+B n^2}.
$$$$

For a string with tension T_s, Young modulus E, diameter d, and bending
second moment I=πd⁴/64, the coefficient in this convention is:

$$$$
B=\frac{\pi^2EI}{T_sL^2}
=\frac{\pi^3Ed^4}{64T_sL^2}.
$$$$

The physics is visible in the scaling: thicker strings and larger E increase
dispersion, while larger tension and length reduce it. The code does not infer
E, d, or T_s independently; it fits B from partial frequencies and
interpolates B between anchor rows.

The tracker fits the linearized relation:

$$$$
\left(\frac{f_n}{nf_1}\right)^2-1\approx Bn^2.
$$$$

Thus a least-squares regression of the left side against n² has slope B.
This is exactly the np.polyfit(A**2, (F/(A*f1))**2-1, 1) operation in
tools/cal.py:26–38 and the corresponding sequential tracker in
tools/evaluate_corpus.py:93–130.

### 4.4 Damping law

The production law is:

$$$$
\sigma_n=b_1+b_3\omega_n^2.
$$$$

b1 controls the long, low-frequency tail. b3 controls how quickly upper modes
disappear. Because ω_n² grows approximately as n² and acquires additional
stiff-string growth, a single b3 per interpolated register can produce very
steep high-frequency decay.

This law is deliberately economical. It is also a known limitation: some
4–6 kHz portions are over-damped while other upper-register windows are still
floor-limited. Adding an arbitrary EQ would conceal the shape error rather than
identify whether the damping law, force pulse, or pickup chain is responsible.

## 5. Bridge-force readout

At the rigid termination x=L, the transverse slope is:

$$$$
\left.\frac{\partial y}{\partial x}\right|_{x=L}
=\frac{\pi}{L}\sum_{n=1}^{\infty}(-1)^n n q_n(t).
$$$$

Multiplying by tension gives the transverse termination force:

$$$$
F_{\mathrm{bridge}}(t)
=T_s\left.\frac{\partial y}{\partial x}\right|_{x=L}
=\frac{T_s\pi}{L}\sum_{n=1}^{\infty}(-1)^n n q_n(t).
$$$$

This explains three source lines that must not be “simplified” independently:

- (-1)^n is the sign from cos(nπ);
- n is the within-note partial weight;
- T_sπ/L is a note-level scale.

The project explicitly keeps n, not f_n/f_0. Replacing n by the dispersed
frequency ratio introduces an extra √(1+Bn²) and made the upper bank too bright
by as much as roughly 7 dB.

For a string, the fundamental relation is:

$$$$
f_0=\frac{1}{2L}\sqrt{\frac{T_s}{\mu}},
\qquad
\mu=\frac{m_{\mathrm{single}}}{L},
\qquad
\frac{T_s}{L}=4m_{\mathrm{single}}f_0^2.
$$$$

bridgeScale at src/cp80.hpp:292–295 is the dimensionless version normalized
to a C4 reference mass and frequency:

$$$$
S_{\mathrm{bridge}}
=\frac{(m/n_{\mathrm{strings}})f_0^2}
{m_{\mathrm{ref}}f_{\mathrm{ref}}^2}.
$$$$

The division by nStrings matters. specForNote() multiplies mass by one or
two strings so the hammer sees the whole unison, but the bridge-force relation
needs the mass of one string when reconstructing T_s/L.

The modal input and output scales are therefore approximately:

$$$$
b_n\propto\frac{1}{m\omega_n},
\qquad
w_n\propto n\frac{T_s}{L},
$$$$

so their principal frequency dependence is not an arbitrary spectral tilt.
The attack envelope should primarily come from the force pulse, strike comb,
finite-width sinc, and modal damping.

## 6. Impulse-invariant modal discretization

### 6.1 Pole locations

For one damped mode, define:

$$$$
\omega_{d,n}=\sqrt{\omega_n^2-\sigma_n^2},
\qquad
r_n=e^{-\sigma_nT}.
$$$$

The exact discrete poles are r_n e^{±iω_{d,n}T}. The second-order
recurrence used by the engine is:

$$$$
u_n[k]=a_{1,n}u_n[k-1]+a_{2,n}u_n[k-2]+b_nF[k-1],
$$$$

with:

$$$$
a_{1,n}=2r_n\cos(\omega_{d,n}T),
\qquad
a_{2,n}=-r_n^2,
$$$$

and the impulse-invariant force coefficient:

$$$$
b_n=\frac{2}{m}\,g_n\,T\,r_n
\frac{\sin(\omega_{d,n}T)}{\omega_{d,n}}.
$$$$

This is what ca1, ca2, bIn, and sIn represent at lines
317–322. The input delay is intentional: hammer force is evaluated before
the mode state is advanced, and the causal state/output convention is the
discrete equivalent of the z⁻¹ term in the source comment.

### 6.2 Stability

The characteristic roots satisfy:

$$$$
|z_{1,2}|=r_n=e^{-\sigma_nT}<1
\quad\text{whenever}\quad \sigma_n>0.
$$$$

Therefore the linear free-phase bank has no finite-difference CFL condition.
The numerical stability condition is not a string-wave Courant bound; it is
positive damping plus finite floating-point arithmetic. The hammer nonlinearity
is separately protected by kMaxForce=400 N.

### 6.3 Approximations made for speed

The code assumes σ/ω is small. It uses:

$$$$
\omega_d\approx\omega\left(1-\frac12(\sigma/\omega)^2\right),
\qquad
\frac{1}{\omega_d}\approx\frac{1}{\omega}
\left(1+\frac12(\sigma/\omega)^2\right).
$$$$

It also evaluates e⁻ˣ with the cubic Taylor polynomial

$$$$
e^{-x}\approx1-x+\frac{x^2}{2}-\frac{x^3}{6},
$$$$

because x=σT is below about 3×10⁻³ in the active bank. The omitted term is
order x⁴; the source comment estimates an error below 2×10⁻¹² in this
operating range.

The spatial sequences do not call sin(nθ) 256 times. They use the Chebyshev
recurrence:

$$$$
s_{n+1}=2\cos(\theta)s_n-s_{n-1},
\qquad
s_n=\sin(n\theta).
$$$$

The recurrence is carried in double to prevent phase drift. This is both a
performance optimization and a numerical correction: passing large arguments
to a single-precision library sine was measured to lose roughly eight orders
of magnitude relative accuracy in the spatial comb.

### 6.4 Oversampling and decimation

The contact solver runs at fs_int=2 fs_host. The bank is stopped at
0.45 fs_host, not at the internal Nyquist, because modes above host output
Nyquist are removed by the linear output chain and would be wasted work.

The 2:1 decimator is a normalized 31-tap Blackman-windowed halfband FIR. For
offset d from its center, the ideal halfband kernel is:

$$$$
h[d]=\frac{\sin(\pi d/2)}{\pi d}\,w[d],
$$$$

with the center defined by its limiting value and w[d] the Blackman window.
Even offsets vanish, so symmetric folding reduces the arithmetic to the
nonzero pairs plus the center tap. This is the “boring” optimization that is
safe because it changes neither the intended passband nor the physical model.

## 7. Hammer/string contact

### 7.1 Velocity mapping

The normalized MIDI velocity v is mapped to an initial hammer speed:

$$$$
v_h(v)=0.35+5.15v^2\quad\mathrm{m/s}.
$$$$

The quadratic mapping compresses low MIDI velocities and is shared by the
initial hammer state and the impact-rate stiffness law. The fitted reference
velocity is v_ref=0.71.

The rate-dependent contact stiffness is:

$$$$
K(v)=K_0\,s_K
\left(\frac{v_h(v)}{v_h(v_{\mathrm{ref}})}\right)^q,
$$$$

where production uses s_K=0.70 and q=2.25. This is not per-note EQ: it is
one global material/rate law applied to the interpolated K_0 anchor.

### 7.2 Compression and force

The string displacement at the strike point is the modal sum:

$$$$
y_s[k]=\sum_n g_n u_n[k].
$$$$

The hammer compression is:

$$$$
\eta[k]=y_h[k]-y_s[k],
\qquad
[\eta]_+=\max(\eta,0).
$$$$

The production contact law is the lossless power-law contact:

$$$$
F[k]=K[\eta[k]]_+^p,
\qquad p=2.
$$$$

The power p is a global material exponent. Historical literature values for
felt hammers are higher, but the CP-80 specification describes urethane plus
artificial leather. The PP-to-FF attack-growth test selected one global p=2
within the available corpus; it is not licensed to vary independently at every
note.

The optional Hunt–Crossley-style branch is:

$$$$
F[k]=K[\eta[k]]_+^p
\max\!\left(0,1+c\left(v_h[k]-v_s[k]\right)\right).
$$$$

It adds rate-dependent loss, but its measured velocity trend moved the wrong
way for the current target and it remains diagnostic.

### 7.3 Hammer integration

With hT the internal sample period and m_h the hammer mass, the explicit
contact update is:

$$$$
v_h[k+1]=v_h[k]-\frac{F[k]}{m_h}hT,
\qquad
y_h[k+1]=y_h[k]+hT\,v_h[k+1].
$$$$

The string modes receive the same force in their next state update. Contact
ends when the hammer has separated and is moving away, or after a hard age
limit of 2000 internal samples. The latter is a safety ceiling, not a physical
claim.

### 7.4 The opt-in Maxwell facing branch

The hard-facing experiment adds a relaxation state Q driven by changes in
the compressed power g(η)=η^p:

$$$$
Q[k]=e^{-hT/\tau}Q[k-1]
+K_f\left(g(\eta[k])-g(\eta[k-1])\right),
$$$$

$$$$
F[k]=K g(\eta[k])+Q[k].
$$$$

This is a compact way to create a sharper force edge from a urethane/leather
face without adding a synthetic noise click. It is not part of the accepted
default because a shared setting matched one register and missed another.

## 8. Unisons and the former 11 dB error

### 8.1 Two resonators below the cutoff

Above MIDI 42 the CP-80 notes are double-strung. For each partial below
kTwinBelow=40, the model emits a slow and a fast partner. If the partners
had exactly the same frequency, their sum would be:

$$$$
y(t)=\left(A_s e^{-\sigma_s t}+A_f e^{-\sigma_f t}\right)
\cos(\omega t+\phi).
$$$$

That is only a monotone sum inside the cosine. It has two decay constants but
no beating nulls. Splitting the frequencies gives a cross term:

$$$$
|Y(t)|^2\sim A_s^2e^{-2\sigma_s t}+A_f^2e^{-2\sigma_f t}
+2A_sA_f e^{-(\sigma_s+\sigma_f)t}
\cos\!\left(2\pi(f_2-f_1)t\right).
$$$$

The beat frequency is the frequency separation |f_2-f_1|, not the half-split
used internally by the code.

### 8.2 Constant-cent unison mistuning

For a small mistuning of c cents:

$$$$
\frac{\Delta f}{f}=2^{c/1200}-1
\approx\frac{c\ln 2}{1200}
=\frac{c}{1731.234}.
$$$$

The code emits f−dHz and f+dHz with:

$$$$
dHz=\frac12 f\frac{c}{1731.234}.
$$$$

The full pair separation is therefore approximately f c/1731.234; it grows
with partial index. This agrees with the reference doublets, whose separation
is approximately constant in cents rather than constant in hertz.

The note-specific interval is deterministic:

$$$$
h(m)=\operatorname{frac}(0.61803398875m),
\qquad
c(m)=0.4+(2.0-0.4)h(m)\ \mathrm{cents}.
$$$$

The golden-ratio fractional sequence is not claimed to reproduce each string's
actual tuning. It is a deterministic decorrelator inside the measured
0.4–2.0 cent range, so a chord does not make every note sweep the same comb.

### 8.3 Energy-preserving truncation

Below the cutoff, a partial carries total bridge weight
slowW + kFastPartnerWeight. Above it, the fast partner is not emitted as a
separate state, but the single proxy carries that same total weight:

$$$$
w_{n>40}=(w_{\mathrm{slow}}+w_{\mathrm{fast}})n_{\mathrm{strings}}.
$$$$

This is a computational truncation. It discards the fine temporal beating above
partial 40 but does not discard the partial's nominal bridge energy. The old
code kept only slowW above the cutoff, creating an artificial:

$$$$
20\log_{10}\!\left(\frac{slowW+1.15}{slowW}\right)
$$$$

step at a different frequency for every note. That was the hidden reason D#1
looked dull while F#2 looked too bright; it was not a hammer-register law.

Single-strung notes retain kPolarHz=0 because the evidence for a clean
polarization split exists only for one bass fundamental. Adding it globally
made F#2 beat when its reference did not.

## 9. Note specification and interpolation

### 9.1 Pitch and stretch

Equal-tempered MIDI pitch is:

$$$$
f_{\mathrm{ET}}(m)=440\,2^{(m-69)/12}.
$$$$

The CP-80 specification supplies a Railsback-like stretch curve in cents. A
cents offset is converted to a multiplicative frequency factor:

$$$$
f_0(m)=440\,2^{\left(m-69+0.01\,c(m)\right)/12},
$$$$

where c(m) includes the printed stretch and any diagnostic global tuning
offset. The inverse definition is:

$$$$
c=1200\log_2\!\left(\frac{f}{f_{\mathrm{ET}}}\right).
$$$$

The negative bass and positive treble offsets are treated as an instrument
specification, not automatically as bad tuning in the recordings.

### 9.2 Linear versus logarithmic interpolation

Between two anchor rows A and B, the normalized coordinate is:

$$$$
t=\operatorname{clip}\!\left(\frac{m-m_A}{m_B-m_A},0,1\right).
$$$$

Quantities that are additive in their physical scale use linear interpolation:

$$$$
q(t)=(1-t)q_A+tq_B.
$$$$

Positive scale quantities spanning orders of magnitude use log interpolation:

$$$$
q(t)=\exp\!\left((1-t)\ln q_A+t\ln q_B\right)
=q_A^{1-t}q_B^t.
$$$$

The current code uses log interpolation for B, b3, hammer K, and string
mass; linear interpolation for strike position, b1, hammer mass, exponent,
damper rate, gain, contact width, partner damping, and slow weight. The string
count is a physical step: one string through MIDI 42, two above.

That choice is a modeling assumption. Smooth interpolation is preferable to
per-note lookup when the parameter is genuinely register-dependent, but it
cannot represent every local manufacturing variation. The reference shows a
small per-note tuning scatter; the production model intentionally does not
turn each sample into its own correction.

### 9.3 Anchor identifiability

kAnchors is the practical parameter vector. It contains rows at MIDI
21, 36, 48, 60, 72, 84, and 108. The calibration script currently evaluates
target recordings at MIDI 27, 42, 50, 60, 72, 85, and 107 and applies updates
to the corresponding anchor indices. Thus the rows at 84 and 108 are being
informed by neighboring treble recordings rather than directly identified at
their exact note numbers. Older documentation calls several upper anchors
“unconstrained”; that warning remains useful. A fitted value is not the same
thing as a directly measured material constant.

## 10. Shared frame/body modes

The low-frequency signature in the reference is not a string partial for many
mid/high notes. The production approximation is four shared low-Q resonators:

| mode | frequency | sigma | relative weight |
|---:|---:|---:|---:|
| 1 | 38 Hz | 8 1/s | 1.00 |
| 2 | 80 Hz | 8 1/s | 0.30 |
| 3 | 170 Hz | 8 1/s | 0.48 |
| 4 | 32 Hz | 4 1/s | 0.20 |

Each body mode uses the same second-order recurrence as a string mode. At
prepareBody() the impulse coefficient is first computed as:

$$$$
b_{0,i}=T r_i\frac{\sin(\omega_{d,i}T)}{\omega_{d,i}},
$$$$

then the code simulates four seconds and divides by the observed peak:

$$$$
b_i=\frac{b_{0,i}}{\max_k|y_i[k]|}.
$$$$

This makes bodyGain an interpretable mix rather than an accidental
4×10⁻¹⁰ displacement scale.

The body impulse is:

$$$$
I_{\mathrm{body}}
=G_{\mathrm{body}}k_{\mathrm{body}}m_h\,v_{\mathrm{body}}
\frac{m_{\mathrm{string,total}}}{m_{\mathrm{body,ref}}},
$$$$

with the impact-weighted speed law:

$$$$
v_{\mathrm{body}}
=v_{\mathrm{ref}}
\left(\frac{v_h(v)}{v_h(v_{\mathrm{ref}})}\right)^{1.25}.
$$$$

Each note schedules this impulse with a deterministic delay between 0 and
15 ms:

$$$$
d_m=\left\lfloor h(m)(N_{\mathrm{delay}}+1)\right\rfloor.
$$$$

This is a deliberately small surrogate for different frame arrival paths. It
prevents a chord from adding every shared-mode impulse at exactly the same
sample. The earlier alternating-sign fix was rejected: balanced chords
cancelled the body by about 15 dB. Delay decorrelation is less pathological.

The body is still not a cast-iron modal model. It has no spatial mode shapes,
bridge attachment coordinates, or energy exchange with string modes. Its
physical content is the measured common frequency/decay signature and the
impact/mass scaling, not a complete frame mechanics derivation.

## 11. Electrical output chain

### 11.1 Pickup bandwidth and AC coupling

The production medium brilliance setting uses lp=4400 Hz and hp=32 Hz.
Two one-pole low-pass poles are placed at lp and 1.7 lp:

$$$$
k_1=e^{-2\pi f_{\mathrm{lp}}/f_s},
\qquad
k_2=e^{-2\pi(1.7f_{\mathrm{lp}})/f_s}.
$$$$

The equivalent two-pole recurrence in processBlock() is:

$$$$
y[k]=b_0x[k]+(k_1+k_2)z_1[k-1]-k_1k_2z_2[k-1],
\qquad
b_0=(1-k_1)(1-k_2).
$$$$

AC coupling is a one-pole high-pass implemented by subtracting a smoothed
copy:

$$$$
h[k]=y[k]+a_h(h[k-1]-y[k]),
\qquad
v[k]=y[k]-h[k],
\qquad
a_h=e^{-2\pi f_{\mathrm{hp}}/f_s}.
$$$$

The low/medium/high brilliance corners are 3.3, 4.4, and 8.0 kHz. They are
plausible panel-level settings, not a transcription of the CP-70B RC network.

### 11.2 Tone stack

The three panel filters are a low shelf at 150 Hz, a peaking filter at 800 Hz,
and a high shelf at 3 kHz. For the middle peaking section, the code uses the
standard biquad quantities:

$$$$
A=10^{G/40},
\qquad
\omega=2\pi f/f_s,
\qquad
\alpha=\frac{\sin\omega}{2Q},
$$$$

$$$$
a_0=1+\alpha/A,
\quad
b_0=\frac{1+\alpha A}{a_0},
\quad
b_1=\frac{-2\cos\omega}{a_0},
\quad
b_2=\frac{1-\alpha A}{a_0},
$$$$

$$$$
a_1=b_1,
\qquad
a_2=\frac{1-\alpha/A}{a_0}.
$$$$

The state update is transposed direct-form II:

$$$$
y=b_0x+z_1,
\qquad
z_1=b_1x-a_1y+z_2,
\qquad
z_2=b_2x-a_2y.
$$$$

The hardware has a center detent, so the engine default is now 0/0/0 dB.
The historical −5 and −8 dB middle cuts were corpus compensation and were
removed from the default. The optional light EQ is an offline diagnostic,
not part of the physical string engine:

$$$$
y=x+(10^{0.55/20}-1)x_{20\text{--}250}
 +(10^{-0.75/20}-1)x_{600\text{--}1600}
 +(10^{1.20/20}-1)x_{2500\text{--}6500}.
$$$$

It is a useful description of a small chord-derived residual, but shipping it
would make the plugin dependent on a post-process and would blur whether the
upstream contact/readout model is correct.

## 12. Plugin tremolo and host boundary

The SDK-free adapter splits a host block at every MIDI event offset. If an event
occurs at sample j, the engine processes [0,j), applies the event, then
processes [j,n). This is why note timing does not depend on the host buffer
size. The adapter also linearly ramps volume over each block:

$$$$
g[i]=g_0+\frac{i+1}{n}(g_1-g_0).
$$$$

The JUCE wrapper owns the parameter snapshot and sends note/pedal events to the
adapter. It applies a fixed 44x line-output calibration after the model and
tremolo. That is approximately:

$$$$
20\log_{10}(44)=32.87\ \mathrm{dB}.
$$$$

The gain is a plugin electrical-boundary calibration, not a change to the
modal model or the offline waveform comparisons.

With tremolo enabled, depth d is a fraction and θ advances at the selected
speed:

$$$$
\theta[k+1]=\theta[k]+\frac{2\pi f_{\mathrm{trem}}}{f_s},
$$$$

$$$$
L[k]=x[k]\left(1-\frac d2+\frac d2\cos\theta[k]\right),
\qquad
R[k]=x[k]\left(1-\frac d2-\frac d2\cos\theta[k]\right).
$$$$

The modulation is antiphase, not a stereo pan. The sum is:

$$$$
L[k]+R[k]=(2-d)x[k].
$$$$

The plugin smoke test checks this identity, finite output, tremolo-off dual
mono, and nonzero dry level. pluginval then checks the VST3 across common
sample rates, block sizes, automation, state, bus layouts, and processing
lifecycles; auval is the separate AU check.

## 13. What the waveform metrics actually compute

### 13.1 FFT convention

For a windowed segment x_j with Hann window w_j, the analysis spectrum is:

$$$$
X_k=\sum_{j=0}^{N-1}w_jx_j e^{-i2\pi kj/N},
\qquad
f_k=\frac{k f_s}{N}.
$$$$

The corpus evaluator uses a 2¹⁸-point real FFT for fixed frequency-bin
resolution. The short attack and longer sustain windows are zero-padded to the
same FFT size, so the H1 reference does not silently change because the window
length changed.

The exact absolute FFT normalization is unimportant for most comparisons,
because the band values and H1 use the same scale. The window still affects
leakage and therefore affects a weak high partial.

### 13.2 H1-relative band energy

Let A_1 be the measured H1 peak amplitude in the attack window. For a band
B=[f_l,f_h), the evaluator computes:

$$$$
P_{\mathcal B}=\sum_{k:f_k\in\mathcal B}|X_k|^2,
$$$$

$$$$
L_{\mathcal B}=10\log_{10}\!\left(\frac{P_{\mathcal B}}{A_1^2}\right).
$$$$

This is a power-band level referenced to an amplitude peak squared. It is not
the RMS dBFS of the whole file. The useful consequence is that a per-file
recording gain cancels. The cost is that the reference H1 itself can be a bad
normalizer in a clipped, body-heavy, or strongly beating attack.

The standard spectral bands are:

- 100–1000 Hz
- 1000–2000 Hz
- 2000–3000 Hz
- 3000–4000 Hz
- 4000–6000 Hz
- 6000–9000 Hz

They are measured in two windows:

- attack: 0–30 ms;
- sustain: 200–500 ms.

The additional body bands are 20–60, 60–120, and 120–220 Hz. Body values are
also relative to the attack H1. This makes a 20 Hz frame mode measurable even
when the note's own fundamental is 261 Hz or higher.

### 13.3 Tail SNR gate

The reference file's late tail is used as a noise estimate. For an attack band
power P_a and tail power P_t:

$$$$
\mathrm{SNR}_{\mathrm{tail}}
=10\log_{10}\!\left(\frac{P_a}{P_t}\right).
$$$$

The tail is the final 400 ms, ending 20 ms before EOF, with a minimum 50 ms
separation from the measured window. If a reference attack is less than 12 dB
above its own tail in a band, that band is flagged and excluded from the
valid metrics and the priority score.

This gate is one of the most important pieces of code in the project. Without
it, the fitter tried to reproduce tape/room hiss above 8 kHz and produced a
false EQ target. The model has sparse deterministic partials; a recording has
a stationary floor between partials. Those are not the same physical object.

### 13.4 Decay sigma

spectral_balance.py bandpass-filters the whole waveform with a sixth-order
Butterworth filter, computes the analytic signal with a Hilbert transform,
and takes its magnitude:

$$$$
z(t)=\operatorname{Hilbert}\{x_{\mathcal B}(t)\},
\qquad
E(t)=|z(t)|.
$$$$

For an exponentially decaying amplitude:

$$$$
E(t)=E_0e^{-\sigma t}
\quad\Longrightarrow\quad
\ln E(t)=\ln E_0-\sigma t.
$$$$

The script averages the envelope over 2 ms blocks, discards samples below 45 dB
relative to the band peak, and fits the slope:

$$$$
\widehat\sigma=-\operatorname{slope}\bigl(t,\ln E(t)\bigr).
$$$$

This is an amplitude-decay constant. A power envelope would decay as
e⁻²σt and would give a factor-of-two different slope. The project always
labels the estimator in 1/s to avoid that ambiguity.

The high bands are floor-limited: extending a fit window after the partial has
fallen into recording noise makes the apparent sigma artificially small. That
is why the standard window is 50–300 ms and why the tail gate is separate.

### 13.5 Partial tracking and inharmonicity

The tracker first searches for H1 in a broad 0.94 f_0 to 1.06 f_0 window.
For each next partial it predicts:

$$$$
\widehat f_n=f_1n\sqrt{1+Bn^2},
$$$$

searches within a tolerance, takes the strongest local peak, and rejects peaks
below −80 dB relative to H1 in the corpus evaluator.
It then refits B sequentially using the linearized relation from section 4.3.

The sequential step matters. Starting every partial search at B=0 makes the
tracker look at the wrong frequency by high n; a false peak then contaminates
the next prediction. This was a measurement failure, not a model failure.

The harmonic summary retains tracked partials above −70 dB relative to H1 and
fits a straight line to amplitude versus log partial index:

$$$$
\ell_n=20\log_{10}\!\left(\frac{A_n}{A_1}\right),
\qquad
\ell_n\approx\alpha+\beta\log_2 n.
$$$$

The fitted β is reported as spectral tilt in dB/octave. The intercept is not
the important quantity when H1-relative levels are already used; the slope
reveals whether the model is losing or retaining energy too quickly with
partial number.

### 13.6 Pitch and pitch glide

H1 is re-found in the 30–200 ms, 200–500 ms, and 1–2 s windows. For nominal
equal-tempered frequency f_ET, pitch offset is:

$$$$
c(t)=1200\log_2\!\left(\frac{f_1(t)}{f_{\mathrm{ET}}}\right).
$$$$

The reported glide is more robustly measured relative to the early window:

$$$$
\Delta c_w=1200\log_2\!\left(\frac{f_{1,w}}{f_{1,\mathrm{early}}}\right).
$$$$

The production model has a static Railsback curve. FF tension glide is a real
reference observable but remains deferred because it requires time-varying
string tension/contact coefficients and is a small return for the current goal.

### 13.7 Beating / amplitude modulation

For each of H1–H6, the sustain signal is bandpassed in a narrow interval around
the tracked partial, then Hilbert-demodulated. Let e_dB(t)=20log10(E(t)).
The code removes a least-squares linear decay trend and reports:

$$$$
\mathrm{AM}_{\mathrm{rms}}
=\sqrt{\frac{1}{M}\sum_{j=1}^{M}
\left(e_{\mathrm{dB}}(t_j)-\widehat e_{\mathrm{trend}}(t_j)\right)^2}.
$$$$

This measures envelope fluctuation in dB, not a unique physical string split.
It is useful for detecting the old degenerate-unison mistake and the later
phase-locking mistake in chords. It is sensitive to bandpass width, Hilbert
edge effects, decay-trend choice, and the fact that two-string partners do not
exchange energy in the current model.

### 13.8 Per-file errors

For any scalar observable q, the evaluator stores the signed model-minus-
reference residual:

$$$$
e_q=q_{\mathrm{model}}-q_{\mathrm{reference}}.
$$$$

For a finite set of residuals e_i, the row RMSE is:

$$$$
\mathrm{RMSE}(e)=\sqrt{\frac{1}{N}\sum_{i=1}^{N}e_i^2}.
$$$$

The sign is retained in individual columns because a bias matters. The RMSE is
used for ordering because it penalizes large misses without allowing positive
and negative errors to cancel.

For H1–H12, the harmonic residual is in dB relative to each file's H1. The
harmonic bias is also stored:

$$$$
\mathrm{bias}(e)=\frac{1}{N}\sum_i e_i.
$$$$

A negative bias says the model is generally too weak; a slope in residual versus
log₂(n) says the physical envelope has the wrong shape.

### 13.9 Corpus priority score

For a sample row, the diagnostic priority score is the RMS of six component
RMSEs:

$$$$
P=\sqrt{\frac{1}{6}\left(
R_A^2+R_S^2+R_{BA}^2+R_{BS}^2+R_{HA}^2+R_{HS}^2\right)}.
$$$$

Here A/S are attack/sustain spectral-band RMSEs, BA/BS are body-band RMSEs,
and HA/HS are H1–H12 harmonic RMSEs. Missing or invalid components are
omitted by the finite-value helper, so the effective denominator can be less
than six.

The aggregate score is the arithmetic mean of row scores within all samples,
velocity layers, or register groups. It does not weight samples by duration,
perceptual importance, or physical energy. It is therefore a ranking device:

> Use priority_score_db to find where to measure next; do not blindly minimize
> it by inventing a per-note parameter.

## 14. Musical/chord metrics

An isolated sample can be peak-matched and still sound wrong in a chord.
The canonical demo has four staggered six-note chords plus an upper-register
figure. evaluate_demo.py resamples both files to 44.1 kHz, applies sixth-order
zero-phase Butterworth bands, and measures RMS:

$$$$
\mathrm{RMS}_{\mathcal B}
=20\log_{10}\!\left(
\sqrt{\frac{1}{M}\sum_{j\in\mathcal B}x_j^2}
\right).
$$$$

The chord bands are 20–60, 60–120, 120–220, 120–2000, 2000–4000, and
4000–8000 Hz. It also measures modulation in 300–3000, 1000–5000, and
3000–8000 Hz with the same detrended Hilbert-envelope RMS idea.

The model/reference delta is:

$$$$
\Delta_{\mathcal B}=\mathrm{RMS}_{\mathcal B,\mathrm{model}}
-\mathrm{RMS}_{\mathcal B,\mathrm{reference}}.
$$$$

This is where the alternating body-polarity experiment failed: every isolated
note looked reasonable, but a balanced chord drove the shared body state toward
zero. It is also where a globally identical unison split sounded like a
flanger: the individual AM depth was plausible, but the chord combs were too
correlated.

## 15. Level-matching experiments and their mathematics

### 15.1 Per-file peak matching

render_reference_match.py renders the model at the SFZ layer midpoint, resamples
it to the reference sample rate, and trims/pads it to exactly the reference
duration. It then chooses:

$$$$
g_{\mathrm{peak}}=
\frac{\max|x_{\mathrm{ref}}|}{\max|x_{\mathrm{model}}|},
\qquad
x_{\mathrm{matched}}=g_{\mathrm{peak}}x_{\mathrm{model}}.
$$$$

This is the correct A/B for timbre independent of that file's recorder gain.
It is not the correct test for whether D#1, C4, and B7 have the same relative
level as the real instrument.

### 15.2 One global library gain

render_lib.py uses one gain across all rendered notes and layers:

$$$$
g_{\mathrm{global}}=\frac{0.89}{\max_{m,\ell,t}|x_{m,\ell}(t)|}.
$$$$

This preserves model-relative register and velocity level, but it does not
correct for the sample pack's file-by-file normalization.

### 15.3 Inverse-volume reference demo

The reference demo can use model peak measurements to scale each replayed
reference voice:

$$$$
g_{\mathrm{inverse}}(m,v)=
\frac{A_{\mathrm{model}}(m,v)}{A_{\mathrm{reference\ sample}}}.
$$$$

It then applies the same 11-second score, resamples nearby notes by a rational
factor, applies a release ramp, sums voices, and normalizes the final demo peak
to 0.89. This creates a fair “what would the reference sound like at the
model's per-voice level?” comparison. It does not make the reference and model
physically calibrated in absolute volts.

### 15.4 SFZ metadata

evaluate_corpus.py parses the companion CP80.sfz for volume, lokey, hikey,
and layer metadata. Those volume entries are useful evidence about the sample
pack's intended register balance, but they are not an independent measurement
of the CP-80's bridge voltage. This is why they are recorded as metadata rather
than silently folded into every error metric.

## 16. Optimization methods actually used

### 16.1 Fixed-point calibration of decay and dispersion

cal.py first measures targets on forte files and then repeats six passes.
The target vector at each selected note contains:

$$$$
\mathbf q^*=(\sigma_{\mathrm{slow}},
\sigma_{\mathrm{fast}},
\mathrm{tilt},B,f_1,
\operatorname{median}(\sigma_{2\text{--}4\,\mathrm{kHz}})).
$$$$

The production update rules are intentionally scalar and clipped.

For b1:

$$$$
b_1^{new}=b_1^{old}\,
\operatorname{clip}\!\left(
\frac{\sigma^*_{\mathrm{slow}}}{\max(\widehat\sigma_{\mathrm{slow}},10^{-3})},
0.4,2.5\right).
$$$$

For the fast partner:

$$$$
r^*_{fs}=\frac{\sigma^*_{\mathrm{fast}}}{\sigma^*_{\mathrm{slow}}},
\qquad
\widehat r_{fs}=\frac{\widehat\sigma_{\mathrm{fast}}}
{\max(\widehat\sigma_{\mathrm{slow}},10^{-3})},
$$$$

$$$$
\mathrm{twinDamp}^{new}
=\operatorname{clip}\!\left(
\mathrm{twinDamp}^{old}
\operatorname{clip}\!\left(\frac{r^*_{fs}}{\max(\widehat r_{fs},0.01)},0.5,2.0\right),
1.05,9.0\right).
$$$$

For b3, the script uses the excess decay above the slow component:

$$$$
b_3^{new}=b_3^{old}\,
\operatorname{clip}\!\left(
\frac{\sigma^*_{2\text{--}4k}-\sigma^*_{\mathrm{slow}}}
{\max(\widehat\sigma_{2\text{--}4k}-\widehat\sigma_{\mathrm{slow}},10^{-3})},
0.5,2.0\right).
$$$$

For inharmonicity:

$$$$
B^{new}=B^{old}\,
\operatorname{clip}\!\left(\frac{B^*}{\widehat B},0.5,2.0\right).
$$$$

The hammer width is only clamped:

$$$$
hW\leftarrow\operatorname{clip}(hW,0.002,0.02).
$$$$

The algorithm works because each selected measurement is approximately more
sensitive to one parameter than the others. It is not a gradient method and
it does not prove global convergence. Clipping prevents a bad peak tracker from
turning one row into an absurd anchor.

Important omission: tilt is printed and inspected but is not directly used in
the update. That is deliberate after the old fitter drove contact width to an
unphysical value in an attempt to repair a damping/attack-coordinate error.

### 16.2 Hammer exponent grid gate

fit_hammer.py tests a global p over a small grid. It compresses the attack
bands into four values: 0.1–1, 1–2, combined 2–4, and 4–6 kHz. Combining 2–3
and 3–4 kHz is done in the power domain:

$$$$
L_{2\text{--}4}
=10\log_{10}\!\left(
10^{L_{2\text{--}3}/10}+10^{L_{3\text{--}4}/10}
\right).
$$$$

For each note it compares the loudest available layer with the softest:

$$$$
\Delta L_{n,b}=L_{n,b}^{high}-L_{n,b}^{low}.
$$$$

The global loss is the mean squared error over notes and bands:

$$$$
J(p)=\frac{1}{N}\sum_n\frac{1}{4}\sum_b
\left(\Delta L_{n,b}^{model}(p)-\Delta L_{n,b}^{ref}\right)^2.
$$$$

Taking a layer difference cancels most per-file gain. The script reports the
best grid point but never edits anchors automatically. Current production uses
the global p=2 candidate and a global rate-hardening q=2.25.

### 16.3 Prescribed force-pulse fit

pulse_sweep.cpp deliberately removes the hammer contact solver and drives the
existing modal bank with a normalized beta-shaped pulse:

$$$$
F(u;\rho,\phi)=
\frac{u^{\rho}(1-u)^{\phi}}
{u_*^{\rho}(1-u_*)^{\phi}},
\qquad
u=\frac{k}{K-1},
\qquad
u_*=\frac{\rho}{\rho+\phi}.
$$$$

The parameters are duration, rise exponent, and fall exponent. The fitting
target is the first eight harmonic ratios in a 25–240 ms window:

$$$$
r_h=20\log_{10}\!\left(\frac{A_h}{A_1}\right),
\qquad h=2,\ldots,8.
$$$$

The weighted loss is:

$$$$
J=\sum_{h=2}^{8}w_h^2(r_h^{model}-r_h^{ref})^2,
$$$$

with weights 1.5, 1.5, 1.2, 1.0, 0.6, 0.3, 0.15. The optimizer is a
coarse grid followed by three shrinking local grids. This is not a measured
hammer-force reconstruction. It asks a narrower question: can the existing
modal bank explain the early harmonic ladder if the force event has a different
shape?

### 16.4 Hybrid contact sweep

fit_hybrid.py sweeps four opt-in parameters:

$$$$
\boldsymbol\theta=(Z\text{-scale},c,K\text{-scale},p).
$$$$

For each note/layer it computes the same H2–H8 ratio vector and sums weighted
squared errors. It uses coordinate descent: choose the best value of one
coordinate, then the next, for two passes. This is cheap and interpretable,
but it can settle in a coordinate-wise minimum and cannot identify parameters
that produce nearly the same attack spectrum. The measured improvement from
6013 to 1790 was therefore evidence that the route is promising, not a reason
to promote its best two-note setting to production.

### 16.5 Strike-comb diagnostic

fit_strike.py tries to separate a smooth hammer envelope from the sharp comb
caused by strike position. For candidate x=x_0/L, it subtracts:

$$$$
C_n(x)=20\log_{10}\!\left(
\max\left(|\sin(\pi n x)|,0.03\right)\right)
$$$$

from the measured partial levels. It then fits a quadratic smooth residual:

$$$$
r_n\approx\beta_0+\beta_1\log_2n+\beta_2(\log_2n)^2,
$$$$

and scores the candidate by residual RMS. The floor 0.03 prevents an exact
comb null from making the logarithm singular; it is not a physical noise floor.
This is diagnostic only. A per-layer or per-note best x is not automatically
an instrument law.

## 17. Why several plausible optimizations were rejected

These are not philosophical objections; they are identifiability and
cross-validation failures.

### 17.1 One tilt fit drove hammer width to nonsense

The original tiltof() fitted one spectral slope over a long window. It mixed
the missing high-frequency attack with high-frequency decay and asked hW to
represent both. Since the finite-width factor is a sinc, the fitter increased
the width until it removed too much high-frequency energy. The correct response
was to separate attack windows, decay sigma, tail SNR, and physical contact
width.

### 17.2 Global EQ reproduced the recording floor

The reference above roughly 8 kHz often has attack energy only a few dB above
its own tail. A global EQ cannot distinguish a stationary hiss floor from
missing string modes. The floor gate now excludes such bands; the remaining
high-frequency deficit must be explained by contact force, string dispersion,
damping, or a documented mechanical component.

### 17.3 Alternating body polarity cancelled real chords

For one shared body mode, N identical impulses sum as N, whereas unrelated
arrival phases sum with typical magnitude proportional to √N. Alternating signs
can do worse than √N when the chord contains balanced signs: the sum approaches
zero. Delay spread is a safer low-dimensional approximation.

### 17.4 Upper gain and pickup LP were not enough

Raising upper-register gain improved a raw isolated level comparison but made
the normalized chord body too weak and did not repair the 4–8 kHz timbre. A
global pickup low-pass increase to 8 kHz also failed the full corpus gate. This
is evidence that the missing quality is not a single readout gain; it is likely
an interaction of force-pulse shape, damping, and the pickup/contact coordinate.

### 17.5 Beating must be tested in chords

A deterministic split is physically motivated for each unison. Reusing exactly
the same split on every note phase-locks the combs in a chord. A note-hash spread
preserves the individual-note split while reducing artificial cross-note
coherence. The correct test is isolated AM plus assembled-chord modulation.

### 17.6 The failed numerical shortcuts

- Deferring high modes out of the contact loop was slower because the full
  mode-inner loop vectorized better than a shortened one.
- A hand-approximated fastPow had about 14.8% force-law error and was rejected.
- Nelder–Mead on point-sampled decay envelopes moved zero distance because
  beating nulls made the objective noisy.
- Residual static compliance for truncated modes did not measurably change
  contact time.

The general rule is simple: if an optimizer pushes a physical parameter toward
an absurd value, suspect the coordinate system or estimator before suspecting
the instrument.

## 18. Verification mathematics and gates

### 18.1 Engine stability and speed

make check runs a 30-second dense-note stress test. It records:

- the number of non-finite output samples;
- the maximum absolute output sample;
- peak active mode count;
- realtime factor;
- deliberately oversized blocks at 44.1, 48, and 96 kHz.

The finite-value condition is simply:

$$$$
\forall k,\quad x[k]\in\mathbb R
\quad\text{and}\quad |x[k]|<\infty.
$$$$

The oversized-block test matters because CP80::process() chunks requests into
maxBlockSize; it is a host-boundary invariant, not a timbre metric.

### 18.2 Adapter block invariance

The same event stream is rendered with 64-sample and 1024-sample blocks. The
check computes:

$$$$
e_\infty=\max_k|x_{64}[k]-x_{1024}[k]|,
$$$$

$$$$
e_{\mathrm{rms}}=
\sqrt{\frac{1}{N}\sum_k(x_{64}[k]-x_{1024}[k])^2}.
$$$$

The threshold is e_inf ≤ 5×10⁻⁶; the remaining difference should be roundoff,
not moved MIDI events or block-quantized body impulses.

### 18.3 Plugin validation

pluginval is a host-behavior validator, not a waveform optimizer. Its value
is that it exercises many lifecycles and block/sample-rate combinations that a
single offline render does not. The project uses it as the VST3 gate at strict
level 5, and uses auval for the AU component. A plugin passing pluginval does
not mean its spectrum matches the reference; it means the wrapper is less
likely to break when Reaper changes block size, resets state, automates a
parameter, or reopens the instance.

## 19. Current physical fidelity

The current model is physically meaningful in its core but not a complete
finite-element reconstruction.

### Strong physical content

- exact modal geometry for the chosen simply-supported string basis;
- stiff-string dispersion with fitted B;
- force excitation at the strike point;
- finite hammer contact width as a spatial average;
- nonlinear compression force and hammer momentum update;
- bridge-force readout with the correct within-note n weighting;
- mass/tension scaling through T_s/L=4mf_0²;
- string-count transition at the documented bichord boundary;
- measured constant-cent unison splits;
- modal exponential damping and release/damper changes;
- shared low-Q frame resonances with measured frequencies and a mass/speed drive;
- deterministic delays and no random state in the production unison/body paths.

### Deliberate approximations

- a power-law hammer is not a full urethane/leather constitutive model;
- the force law is not hysteretic in production;
- the two unison partners do not exchange energy through a coupled bridge;
- the frame is four lumped modes, not a spatial cast-iron model;
- tone-stack RC values are plausible placeholders, not transcribed component
  values;
- upper anchors are only partially identified by the available corpus;
- the single b1+b3ω² loss law cannot express every high-band decay shape;
- no mechanical action/noise layer is in the production branch;
- no downstream Twin/desk/chorus/speaker nonlinearity is included.

Those limitations are not defects to hide. They define which new measurement
would justify a new mechanism. A mechanism should be added only when an
existing physical parameter demonstrably cannot move the observable, and when
the proposed addition improves both isolated notes and assembled chords.

## 20. References and provenance

### Sources named by the project

1. A. Chaigne and A. Askenfelt, “Numerical simulations of piano strings I and
   II,” Journal of the Acoustical Society of America, vol. 95, 1994.
   Used for the modal string/damping framework and the register-dependent
   hammer-to-string mass reasoning. The exact paper split and page numbers are
   not pinned in this repository; check the journal copy before citing a table.
2. M. Bank and F. Chabassier, “Model-Based Digital Pianos,” IEEE Signal
   Processing Magazine, vol. 36, no. 1, 2019. Used as a modern overview of
   physical piano synthesis and model-based calibration.
3. M. Bank, Physics-Based Sound Synthesis of the Piano, MSc thesis, Budapest
   University of Technology and Economics. Used for loss-filter calibration,
   beating, and two-stage decay measurement concepts.
4. D. E. Hall and A. Askenfelt, “Piano string excitation V,” Journal of the
   Acoustical Society of America, vol. 83, 1988. Used as context for hammer
   compression exponents and velocity-dependent excitation.
5. A. Askenfelt, ed., Five Lectures on the Acoustics of the Piano, KTH. Used
   as a general acoustics reference for string, hammer, and piano measurements.
6. Euphonics, section 12.2.1, “Piano sound synthesis.” Used as a concise
   comparison point for modal plus IIR implementations.
7. Weinreich’s coupled-string analysis, invoked in AGENTS.md for the
   fast/in-phase and slow/out-of-phase unison interpretation. The repository
   does not record the exact edition or page, so this is a conceptual reference,
   not a fully pinned citation.

### Instrument and data sources

8. Yamaha CP-70B owner’s manual, especially the overall circuit diagram on
   pages 14–15; Yamaha CP-80 documentation for the 88-piezo electrical layout,
   front-panel controls, output level, hammer material, and printed tuning
   curve. The code explicitly marks the tone-stack RC values as untranscribed.
9. Greg Sullivan CP-80 sample set: 81 samples and four velocity layers, named
   NNN-NOTE-PP|MP|F|FF.flac. This is the independent calibration corpus used
   by tools/cal.py, evaluate_corpus.py, and the attack/hammer diagnostics.
10. The companion CP80.sfz file. Its layer velocity ranges and per-sample
    volumes are used as metadata and for level investigations, not treated as
    absolute bridge-voltage calibration.

### Repository implementation references

- src/cp80.hpp: model, anchor table, modal recurrence, hammer, unisons, body,
  pickup, tone stack, and engine process loop.
- tools/cal.py: fixed-point decay/inharmonicity calibration.
- tools/spectral_balance.py: fixed-window H1-relative bands and sigma.
- tools/evaluate_corpus.py: all-sample observables, validity gates, and
  diagnostic priority score.
- tools/evaluate_demo.py: assembled chord RMS and modulation metrics.
- tools/fit_hammer.py, fit_pulse.py, fit_hybrid.py, and fit_strike.py:
  deliberately limited diagnostic optimizers.
- AGENTS.md: measured invariants, rejected experiments, and the project's
  measurement-discipline record.
- PLUGIN.md: host boundary and acceptance checks.
- MECHNOISE.md: deferred mechanical-noise experiment; it is not part of the
  calibrated production tone.

The most important reference is the one produced by measurement itself: every
claim about the recordings must survive a window choice, a noise-floor check,
an isolated-note comparison, and a chord comparison. That is the standard this
document is intended to make reproducible.
