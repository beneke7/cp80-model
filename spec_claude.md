# The CP-80 Model: A Mathematical Account

*Everything here is transcribed from the tree at `c9c986f`, not recalled. Line references are
to `src/cp80.hpp` unless stated. Derivations are carried to the point where the rest is
mechanical; where I have skipped an integral it is because it is a Beta function and you can
look it up.*

---

## 1. The vibrating object

A piano string is a string only to first order. Retain the bending stiffness and you get the
Euler–Bernoulli-corrected wave equation,

$$
\mu\,\frac{\partial^2 y}{\partial t^2}
= T\,\frac{\partial^2 y}{\partial x^2}
- EI\,\frac{\partial^4 y}{\partial x^4},
$$

with $\mu$ the linear density, $T$ the tension, $E$ Young's modulus and $I$ the second moment
of area. Take hinged ends — the correct boundary condition is somewhere between hinged and
clamped, and nobody has ever been able to tell by ear — so that $\sin(n\pi x/L)$ remains an
eigenfunction. Substituting $y=\sum_n q_n(t)\sin(n\pi x/L)$ diagonalises the operator and
leaves

$$
\omega_n^2 = \frac{T}{\mu}\left(\frac{n\pi}{L}\right)^{2}
\left[1+\frac{EI}{T}\left(\frac{n\pi}{L}\right)^{2}\right],
$$

which is the whole of §1 in one line:

$$
\boxed{\;\omega_n=\omega_1\,n\sqrt{1+Bn^2}\;},\qquad
B=\frac{\pi^2EI}{TL^2}.
$$

For a cylindrical core of diameter $d$, $I=\pi d^4/64$, and using $T=4\mu L^2f_0^2$,

$$
B=\frac{\pi^3Ed^4}{256\,\mu L^4 f_0^2}.
$$

### 1.1 Why this instrument is inharmonic on purpose

$B\propto L^{-4}$ at fixed pitch and gauge. The CP-80's bass strings are 67.9 cm against
roughly 213 cm on a concert grand — a factor $3.14$ in $L$, hence a factor $\approx 97$ in
$B$ if nothing else changed.

Something else changes. To hold pitch on a short string you must raise $\mu$, and since
$B\propto d^4/\mu$, the escape route is to raise $\mu$ *without* raising the core diameter:
wrap the string. Winding buys mass at $d_{\text{core}}$ fixed, and $B\propto d_{\text{core}}^4/\mu$
then falls. This is why bass strings are wound, and it is worth stating plainly because it is
the single design decision that makes a short-scale piano possible at all.

The anchor table lands at $B=2.065\times10^{-3}$ at MIDI 21 against $\sim10^{-4}$ for a
concert grand's A0 — about $20\times$, not $97\times$. The winding ate the rest. The residual
factor of twenty is the CP-80's voice: it is not a defect to be corrected, and any fit that
tries to remove it is fitting the wrong instrument.

---

## 2. Damping

Each mode gets a viscous term, giving the modal oscillator

$$
\ddot q_n+2\sigma_n\dot q_n+\omega_n^2q_n=\frac{2}{m}\,\phi_n\,F(t),
$$

and the model uses the standard two-term loss law

$$
\sigma_n=b_1+b_3\,\omega_n^2 .
$$

The two terms have different physics. $b_1$ is frequency-independent per-mode loss — air
drag and, mostly, energy leaving through the bridge. $b_3\omega^2$ is internal friction in
the wire: for a Kelvin–Voigt solid the loss stress is $\propto\eta\dot\varepsilon$, and
projecting that onto mode $n$ contributes a decay rate $\propto\omega_n^2$. Valette and
Cuesta give the full three-term account including an air-viscosity $\sqrt{\omega}$ term; two
terms is the pragmatic reduction and it is what is fitted here.

### 2.1 What a two-parameter loss law cannot do

Measured $\sigma$ ratios (model / reference) at MIDI 50:

| band | 0.5–1k | 1–2k | 2–3k | 3–4k | 4–6k |
|---|---|---|---|---|---|
| ratio | 1.26 | 0.45 | 0.56 | 0.69 | **2.29** |

The model is *under*-damped from 1–4 kHz and *over*-damped at 4–6 kHz. No choice of
$(b_1,b_3)$ fixes this, because $\sigma(\omega)=b_1+b_3\omega^2$ is monotone increasing and
convex: you cannot be simultaneously too small at 3 kHz and too large at 5 kHz. Either the
true loss law has a third term with the opposite sign of curvature — a bridge-coupling
resonance depressing $\sigma$ in a band would do it — or the 4–6 kHz discrepancy is not
damping at all. This is a genuine model deficiency, not a fit that needs more iterations, and
it should be written down as such rather than absorbed into $b_3$.

---

## 3. Excitation: projection, comb, and patch

The hammer applies force $F(t)$ distributed over a contact patch $g(x)$ normalised to unit
integral. Projecting onto mode $n$ and using $\int_0^L\sin^2(n\pi x/L)\,dx=L/2$ — so the
modal mass is $m/2$ with $m=\mu L$ the total string mass — gives the $2/m$ in the equation
above, together with the shape factor

$$
\phi_n=\int_0^1 g(\xi)\sin(n\pi\xi)\,d\xi .
$$

For a rectangular patch of fractional width $w$ centred at $x_0/L$ this separates exactly:

$$
\boxed{\;\phi_n=\underbrace{\sin\!\left(\frac{n\pi x_0}{L}\right)}_{\text{strike comb}}
\cdot\underbrace{\operatorname{sinc}\!\left(\frac{n\pi w}{2}\right)}_{\text{contact patch}}\;}
$$

with $\operatorname{sinc}(u)=\sin u/u$. The second factor is nothing but the Fourier transform
of the patch: a finite hammer cannot excite a mode whose wavelength is shorter than the
contact width, and the first null sits at $n=2/w$.

`cp80.hpp:330-345` implements exactly this, with `thW` $=\pi w/2$ and `xw` $=n\,$`thW`. Both
$\sin(n\theta)$ sequences come from the Chebyshev recurrence

$$
\sin\big((n{+}1)\theta\big)=2\cos\theta\,\sin(n\theta)-\sin\big((n{-}1)\theta\big),
$$

carried in `double` so the accumulated error stays far below `float` resolution — two
multiply–adds replacing two library `sin()` calls per mode.

**A cautionary note on $w$.** Because $\operatorname{sinc}$ is the only monotone-ish
brightness control in the excitation, $w$ is irresistible as an EQ knob, and it has been
abused as one twice in this project's history: once driven to $0.104$ by a tilt fitter, and
once left at $0.048$ at MIDI 60 — a 29 mm contact patch at C4. `cal.py:82` now clamps it to
$[0.002,0.02]$ with the comment that it is a physical contact width and not a proxy for a bad
decay metric. That clamp is the correct kind of prior.

---

## 4. Discretisation

### 4.1 Impulse invariance

The continuous resonator

$$
H(s)=\frac{1}{(s+\sigma)^2+\omega_d^2},\qquad
\omega_d=\sqrt{\omega^2-\sigma^2}
$$

has impulse response $h(t)=\omega_d^{-1}e^{-\sigma t}\sin\omega_d t$. Sample it,
$h[k]=T\,h(kT)$, and sum the geometric series:

$$
H(z)=\frac{T}{\omega_d}\cdot
\frac{e^{-\sigma T}\sin(\omega_d T)\,z^{-1}}
{1-2e^{-\sigma T}\cos(\omega_d T)\,z^{-1}+e^{-2\sigma T}z^{-2}} .
$$

Read off the recursion the code runs:

$$
y[k]=a_1y[k{-}1]+a_2y[k{-}2]+b\,F[k{-}1],
$$
$$
a_1=2e^{-\sigma T}\cos\omega_d T,\qquad
a_2=-e^{-2\sigma T},\qquad
b=\frac{2}{m}\,\phi_n\,\frac{T}{\omega_d}e^{-\sigma T}\sin\omega_d T .
$$

Compare `cp80.hpp:317-320`: `ca1 = 2*r*ct`, `ca2 = -r*r`, `bIn = (2/mass)*shape*T*r*st*rwd`.
Identical.

**Stability is unconditional.** The poles are at $z=e^{(-\sigma\pm i\omega_d)T}$, so
$|z|=e^{-\sigma T}<1$ for any $\sigma>0$ and any $T$. Unlike a finite-difference scheme there
is no CFL condition to violate, no oversampling requirement for stability, and no way for a
mode to blow up short of a NaN arriving from outside. This is the single best reason to build
a modal synthesiser this way.

### 4.2 The expansions are exact, and here is the bound

Damping is either $b_1$ at low $\omega$ or $b_3\omega^2$ at high $\omega$, and across the
whole bank $q\equiv\sigma/\omega\lesssim3\times10^{-3}$. So

$$
\omega_d=\omega\sqrt{1-q^2}=\omega\left(1-\tfrac12q^2\right)+O(q^4),\qquad
\frac{1}{\omega_d}=\frac{1}{\omega}\left(1+\tfrac12q^2\right)+O(q^4),
$$

with $O(q^4)\sim10^{-11}$ — below `float` epsilon. Likewise $x=\sigma T<3\times10^{-3}$, so
the cubic Taylor $e^{-x}\approx1-x(1-x(\tfrac12-\tfrac{x}{6}))$ has truncation error
$x^4/24\approx3\times10^{-12}$. The code's claim of "error < 2e-12" is correct and slightly
conservative. Two transcendentals per mode become three multiply–adds.

### 4.3 Bank size and the Nyquist ceiling

`kMaxModes = 128` per voice, `kTwinBelow = 40` partials get two resonators each, so the bank
holds partials $1\ldots40$ twinned (80 slots) plus singles to $n\le88$. The generation loop
stops at $f_n\ge0.45f_s^{\text{host}}$ (`cp80.hpp:336`) — the *output* Nyquist, not the
internal one, because the string is linear and the decimator will discard anything above it.
Generating those modes was pure waste, roughly half the bank on low notes.

Note that stiffness rescues the bass: at MIDI 27, $B=2.065\times10^{-3}$ gives
$\sqrt{1+B\cdot88^2}=4.12$, so $n=88$ lands at $88\times38.9\times4.12\approx14$ kHz rather
than the 3.4 kHz a harmonic series would reach. The truncation is not audible where it would
matter most.

### 4.4 Decimation

`Decimator2x` builds a 31-tap windowed-sinc halfband at `prepare()`. The halfband property —
$h[i]=0$ at every even offset from centre — follows from the cutoff sitting exactly at
$f_s/4$: the ideal response $\tfrac12\operatorname{sinc}(d/2)$ vanishes at even $d\neq0$. Of 31
taps, 14 are identically zero and the remainder fold by symmetry, leaving 9 multiplies per
output. A Blackman window ($0.42,\,0.5,\,0.08$) sets the stopband; normalising to unit DC sum
fixes the passband gain. It is FIR, so it cannot misbehave.

### 4.5 What the oversampling is actually for

`kOversample = 2`. The modal bank is linear and already bandlimited by construction, so it
needs no oversampling at all. The only nonlinearity in the system is the contact law
$F=K\eta^p$, whose output spectrum extends past the pulse bandwidth $\sim1/\tau$ with a
rolloff set by the smoothness of the force pulse at contact break. With $\tau\approx2$ ms the
fundamental pulse bandwidth is $\sim500$ Hz, and at $f_s^{\text{int}}=96$ kHz the content
folding back is far below the noise floor of anything else in the chain. Two times is
sufficient; the comment offering 4× "for tighter contact resolution" is about resolving the
contact *event* in time, not about aliasing.

---

## 5. The readout, and an exact cancellation

The pickup is a piezo bar under the bridge, so it measures **transverse force at the string
termination**, not displacement and not velocity:

$$
F_b(t)=T\left.\frac{\partial y}{\partial x}\right|_{x=L}
=T\sum_n q_n\frac{n\pi}{L}\cos(n\pi)
=\frac{\pi T}{L}\sum_n(-1)^n\,n\,q_n .
$$

So the readout weight is $w_n=(-1)^n n$ — exactly, with no fitting freedom whatsoever. The
code carries this as `wOut[i] = wSign * nIdx * outW * bridgeScale` (`cp80.hpp:322`), and the
note-level prefactor follows from $T=4\mu L^2f_0^2$ with $\mu=m_1/L$:

$$
\frac{T}{L}=4m_1f_0^2,
$$

$m_1$ being the *single-string* mass — hence `bridgeScale = (mass/nStrings)*f0²/(...)`
normalised to unity at C4 (`cp80.hpp:294`).

### 5.1 The cancellation, and why EQ is forbidden

Give the string an impulse of momentum $J=\int F\,dt$ short compared with every period. Mode
$n$ receives velocity $\dot q_n(0^+)=\tfrac{2}{m}\phi_n J$ and thereafter rings at amplitude

$$
|q_n|=\frac{2\phi_nJ}{m\,\omega_n}\;\propto\;\frac{1}{\omega_n}.
$$

The bridge force weights that by $n$, and $\omega_n\approx n\omega_1$ for small $B$:

$$
\big|F_b^{(n)}\big|\;\propto\;n\cdot\frac{1}{n\omega_1}\;=\;\frac{1}{\omega_1}.
$$

**A delta-function hammer on an ideal string produces a flat bridge-force spectrum.** The
$1/\omega_n$ of the excitation and the $n$ of the piezo readout cancel identically.

This is the central structural result of the whole model, and it has a hard consequence:
*every* feature of the spectral envelope must come from one of exactly three places —

1. the finite duration of $F(t)$, which rolls off above $\sim1/\tau$;
2. the strike comb $\sin(n\pi x_0/L)$;
3. the contact-patch $\operatorname{sinc}$.

There is no fourth term, and in particular there is no legitimate corrective EQ. If the model
is dull in a register, one of those three is wrong; reaching for a shelf filter guarantees
that the error is never found. The tone stack exists because the *hardware* has one, with
detented centres meaning flat — not as a fitting surface.

### 5.2 Consequences for the pulse

Since the envelope is the pulse spectrum, contact time is the master brightness control. A
half-sine force pulse of duration $\tau$ has magnitude spectrum falling as $f^{-2}$ beyond
$\sim1/\tau$; combined with the $+6$ dB/octave from the $n$ weighting, the net bridge-force
tilt above the corner is $-6$ dB/octave. At C4 with $\tau=1.88$ ms the corner is at 530 Hz, so
between 750 Hz and 3 kHz — two octaves — you lose 12 dB from the pulse alone. Halving $\tau$
moves the corner up an octave and hands back 6 dB at 3 kHz. This is why the mid-register
brightness deficit was a contact-time problem and not a filter problem, and why a $K$ sweep
moved the 1–4 kHz error at MIDI 57 from 23.5 dB to 7.7 dB while hammer width moved it by 1.1
dB in the wrong direction.

---

## 6. The hammer

### 6.1 The power law and its dimensions

$$
F=K\eta^{\,p},\qquad \eta=y_h-y_s\;\;(\eta>0\ \text{in contact}).
$$

$[K]=\mathrm{N\,m^{-p}}$ — **the units of $K$ depend on $p$.** Therefore $K$ values are not
comparable across different $p$ and no sweep over $p$ at fixed $K$ means anything. The
invariant is the force at a typical compression $\eta_*$; holding $F_*=K\eta_*^p$ fixed under
$p\to p+1$ requires

$$
K\;\longrightarrow\;K/\eta_* .
$$

With $\eta_*\approx1$ mm (computed below), that is three orders of magnitude per unit of $p$.
An early sweep here capped $K$ at $\times256$ and concluded "$p=3$ is catastrophically dull";
redone with $K$ profiled properly, forte matched at every $p$ from 2.0 to 3.5. The lesson is
elementary and worth stating in the language of statistics: **when you scan one parameter you
must re-optimise the nuisance parameters, i.e. take a profile likelihood, not a slice.**

### 6.2 Contact time

Treat the string as immovable (good in the bass, where the hammer is light relative to the
impedance it sees). Then $M_H\ddot\eta=-K\eta^p$, and energy conservation gives

$$
\tfrac12M_H\dot\eta^2+\frac{K\eta^{p+1}}{p+1}=\tfrac12M_Hv_0^2
\;\Longrightarrow\;
\eta_{\max}=\left(\frac{(p{+}1)M_Hv_0^2}{2K}\right)^{\frac{1}{p+1}} .
$$

The contact duration is twice the time to maximum compression,

$$
\tau=2\int_0^{\eta_{\max}}\frac{d\eta}{v_0\sqrt{1-(\eta/\eta_{\max})^{p+1}}}
=\frac{2\eta_{\max}}{v_0}\cdot\frac{1}{p+1}\,
B\!\left(\tfrac{1}{p+1},\tfrac12\right),
$$

the integral being a Beta function after $t=u^{p+1}$. Hence the scaling that matters:

$$
\boxed{\;\tau\;\propto\;\left(\frac{M_H}{K}\right)^{\frac{1}{p+1}}
v_0^{\frac{1-p}{1+p}}\;}
$$

For $p=2$: $\tau\propto(M_H/K)^{1/3}v_0^{-1/3}$, and the Beta factor is
$\tfrac13B(\tfrac13,\tfrac12)=1.402$.

**Numbers at C4**, velocity $0.71$: the action law $v_0=0.35+5.15v^2$ gives $v_0=2.95$ m/s;
with $M_H=4.6$ g and $K\approx7\times10^7$,

$$
\eta_{\max}=\left(\frac{3\times0.0046\times8.7}{1.4\times10^8}\right)^{1/3}
\approx9.5\times10^{-4}\ \mathrm{m},
\qquad
\tau\approx\frac{2\times9.5\times10^{-4}}{2.95}\times1.402\approx0.90\ \mathrm{ms}.
$$

The model measures 1.88 ms. The rigid-string estimate is short by a factor of two, and the
missing factor is precisely the string compliance we threw away: the string recoils under the
hammer, reducing $\ddot\eta$ and extending contact. That the discrepancy is a clean factor of
$\sim2$ rather than something erratic is mild evidence the rest of the arithmetic is right.

### 6.3 Rate hardening

Urethane and leather stiffen with impact rate. The model multiplies

$$
K\;\to\;K_0\left(\frac{v_0}{v_{\text{ref}}}\right)^{q},\qquad q=2.25,
$$

(`kHammerRateQ`, `cp80.hpp:72`, with $v_{\text{ref}}$ taken at MIDI velocity 0.71). Feeding
that into the contact-time law:

$$
\tau\;\propto\;\left(\frac{M_H}{K_0}\right)^{\frac{1}{p+1}}
v_0^{\frac{1-p-q}{1+p}} .
$$

With $p=2,\;q=2.25$ the velocity exponent goes from $-\tfrac13$ to $-1.083$ — **rate hardening
triples the velocity sensitivity of contact time.** That is the entire justification for the
term: the reference's PP→FF growth in the 4–6 kHz band is 71–79 dB, and a bare $p=2$ power law
produces only 25–34 dB. One exponent, one line, and it replaced a planned subsystem.

### 6.4 The two loss branches, and why both are off

**Hunt–Crossley** (`cp80.hpp:461-468`) adds hysteresis through the compression rate,

$$
F=K\eta^{p}\left(1+\alpha\,\dot\eta\right)_+,
$$

so loading raises the force and unloading lowers it, dissipating energy per cycle in a way
that reproduces a velocity-dependent coefficient of restitution. Physically right; measured
here to move the PP→FF growth *monotonically the wrong way* ($+27.6\to+12.1$ dB). `hC` is
therefore zero in production.

**Stulov's hereditary branch** (`cp80.hpp:454-460`) is the more faithful felt model: a Maxwell
arm in parallel with the elastic spring,

$$
\dot Q+\frac{Q}{\tau_f}=K_f\frac{d}{dt}\big(\eta^{p}\big),
\qquad F=K\eta^p+Q,
$$

discretised exactly as `hFastQ = hFastR*hFastQ + fastK*(g - gPrev)` with
$\texttt{hFastR}=e^{-T/\tau_f}$. This gives a sharp stress response to changing compression
followed by exponential relaxation of the facing — the observed asymmetry of real hammer force
pulses. Also off by default, pending a fit against the urethane/leather attack targets.

Both are present, both are correct physics, and both are disabled because the measurements did
not support switching them on. That is the right disposition for a mechanism you believe in
but cannot yet justify.

### 6.5 The wave-junction branch

The alternative contact model treats the strike point as a scattering junction on a digital
waveguide rather than a point on a modal sum. The characteristic impedance of one string is

$$
Z_1=\sqrt{T\mu}=\sqrt{4\mu L^2f_0^2\cdot\mu}=2\mu Lf_0=2m_1f_0 ,
$$

and since a bichord presents two strings in parallel to the hammer, the junction sees
$n_s Z_1 = 2\,m_{\text{total}}f_0$ — which is `waveZ = 2*s.mass*s.f0` (`cp80.hpp:384`). This
looks like a missing factor of two and is not; I checked, having first got it wrong.

At the junction, with incoming velocity waves $v_L^-,v_R^-$ and injected force $F$,

$$
v=\frac{v_L^-+v_R^-}{2}+\frac{F}{2Z},\qquad
v_L^+=v-v_L^-,\quad v_R^+=v-v_R^-,
$$

the standard Kelly–Lochbaum form, delayed by $\lceil x_0/(Lf_0)\cdot f_s\rceil$ and its
complement (`cp80.hpp:385-386`, `519-523`).

Because the junction is implicit — $F$ depends on $\eta$, which depends on the string velocity,
which depends on $F$ — the code solves

$$
x+C\,F(x)=\mathrm{gap},\qquad
C=\frac{T^2}{M_H}+\frac{T}{2Z},
$$

by six Newton steps, $x\leftarrow x-\dfrac{x+CF(x)-\mathrm{gap}}{1+CF'(x)}$, clamped to
$[0,\mathrm{gap}]$ (`cp80.hpp:481-507`). $C$ is the one-step compliance of hammer inertia plus
junction admittance; the clamping is what keeps a Newton solve honest on a power law with
$F'(0)=\infty$ for $p<1$.

Measured: the junction cuts D#1's H2–H6 growl error from $-11.6$ to $-6.2$ dB, but pushes D3
from $+9.8$ to $+17.8$. A bass-only improvement. It is opt-in for exactly that reason.

---

## 7. Coupled strings, beating, and the accidental flanger

### 7.1 The two-oscillator problem

Weinreich's result, reduced to what the model needs. Two partners at $\omega\pm\Delta\omega/2$
with amplitudes $A_{1,2}$ and decays $\sigma_{1,2}$ sum to a signal whose squared envelope is

$$
|y|^2=A_1^2e^{-2\sigma_1t}+A_2^2e^{-2\sigma_2t}
+2A_1A_2e^{-(\sigma_1+\sigma_2)t}\cos(\Delta\omega\,t).
$$

Three readings of one equation:

- $\Delta\omega=0$ **and** $\sigma_1=\sigma_2$: the sum collapses to a single exponential. No
  beat, no double decay, nothing. A degenerate pair is worth exactly one resonator.
- $\Delta\omega\ne0$: beating at $\Delta\omega$, with modulation depth
  $\;20\log_{10}\dfrac{A_1+A_2}{|A_1-A_2|}$ at $t=0$. With the code's $A_1=0.45$ (`slowW`) and
  $A_2=2.0$ (`kFastPartnerWeight`) that is $3.98$ dB — shallow, which is correct; a real
  unison never nulls completely because the two strings are unequally coupled.
- $\sigma_1\ne\sigma_2$: two-stage decay. The strongly bridge-coupled partner dumps its energy
  fast, the other rings on. This is the piano's characteristic prompt-then-aftersound, and it
  is why `twinDamp` exists.

### 7.2 Cents versus hertz: deriving the flanger

A cent is a ratio, so a mistuning of $c$ cents gives

$$
\frac{\Delta f}{f}=2^{c/1200}-1\approx\frac{c\ln 2}{1200}
\quad\Longrightarrow\quad
\frac{f}{\Delta f}=\frac{1200}{\ln 2}=1731.234,
$$

which is where that constant in `cp80.hpp:360` comes from. Now the crucial consequence: if the
two strings are mistuned by a fixed *interval*, then

$$
\Delta f_n=\frac{c\,f_n}{1731.234}\approx\frac{c\,n f_1}{1731.234}\;\propto\;n .
$$

**The beat rate of partial $n$ grows linearly with $n$.** Both partners are launched from zero
state by the same force, so they start in phase, and partial $n$ reaches its first null at

$$
t_n=\frac{1}{2\Delta f_n}=\frac{1731.234}{2c\,n f_1}\;\propto\;\frac1n .
$$

Invert it: at time $t$ the nulls sit at $n(t)=1731.234/(2cf_1t)$ — a set of notches **evenly
spaced in linear frequency**, sweeping downward as $t^{-1}$. That is the definition of a
flanger, and it is what the ear reported.

The older code used a fixed *hertz* detune (`det`, 0.35–2.0 Hz), which makes every partial of a
note beat at the same rate: one coherent wobble, i.e. tremolo. Benign, and physically wrong.
The cents-proportional version is physically right.

What made it pathological was that $c$ was a single global constant, so every note in a chord
swept an identical comb, phase-locked. The fix is not to reduce the depth — the measured
modulation depth already matched the reference to within 0.3 dB — but to decorrelate:

$$
c(\text{note})=c_{\min}+(c_{\max}-c_{\min})\,\big\{\,\text{note}\cdot\varphi^{-1}\big\},
\qquad \varphi^{-1}=0.61803\ldots
$$

(`cp80.hpp:184-192`, $c\in[0.4,2.0]$). The fractional parts of $n\varphi^{-1}$ are the
canonical low-discrepancy sequence — by the three-distance theorem, successive points partition
the interval into at most three distinct gap lengths, so no small set of notes clusters. It is
also stateless, deterministic and free, which is the right price for a dithering scheme.

---

## 8. Summation statistics, or how a correct-looking fix lost 15 dB

$N$ impulses of equal magnitude $a$ and phases $\theta_i$ driving one shared linear resonator
produce amplitude $\big|a\sum_i e^{i\theta_i}\big|$. Three regimes:

$$
\Big|\sum_i e^{i\theta_i}\Big| =
\begin{cases}
N & \theta_i\ \text{equal (coherent)},\\[2pt]
\sqrt{N} & \theta_i\ \text{i.i.d. uniform (incoherent, in r.m.s.)},\\[2pt]
0 & \theta_i\in\{0,\pi\}\ \text{balanced (antiphase)} .
\end{cases}
$$

In decibels for $N=14$: $+22.9$, $+11.5$, $-\infty$.

The shared body bank is driven once per note-on. Fourteen note-ons in the demo therefore stack
coherently at $+22.9$ dB, whereas fourteen overlaid reference *samples* — each carrying its own
independently recorded thump — stack incoherently at $+11.5$ dB. An 11 dB artefact, correctly
diagnosed.

The fix applied was `bodyPolarity = (note & 1) ? -1 : +1`. This moves the system from the first
case to the **third**, not the second. A sign is a phase drawn from $\{0,\pi\}$, and strict
alternation is the maximally balanced such draw: it cancels. Measured consequence — per note
the 20–60 Hz band sits between $-7.3$ and $+5.0$ dB of reference, while in chords it is
$-13.5$ to $-16.6$ dB.

The correct decorrelation is a **phase** spread, not a sign flip: delay each note's body
impulse by $\tau_i\in[0,15\ \mathrm{ms}]$ from the same golden-ratio hash, giving
$\theta_i=\omega\tau_i$ spanning $0$ to $2\pi\cdot38\cdot0.015=3.6$ rad at the lowest body
mode — near enough uniform.

The methodological point deserves its own line, because it is not obvious: **a statistic over
$N$ sources is identically invisible at $N=1$.** No per-note test, however careful, can see
this class of error. It must be tested in a chord.

---

## 9. Estimators

### 9.1 Heterodyne extraction, and why the box filter lied

To follow partial $k$, shift it to DC and low-pass:

$$
z(t)=x(t)\,e^{-i2\pi f_kt},\qquad \text{env}(t)=\big|\,(z*h)(t)\,\big| .
$$

Everything depends on $h$. A rectangular (box) window of length $W$ has

$$
|H(f)|=\left|\frac{\sin\pi fW}{\pi fW}\right|,
$$

with first sidelobe at $-13.3$ dB and an asymptotic rolloff of only $-6$ dB/octave. At MIDI 27
with $W=0.35$ s, a partial at 3.8 kHz sits $\Delta f=3.76$ kHz from the fundamental, so leakage
is suppressed by only

$$
\frac{1}{\pi\,\Delta f\,W}=\frac{1}{\pi\cdot3760\cdot0.35}=2.4\times10^{-4}\;\;(-72\ \mathrm{dB}).
$$

But the fundamental is some $60$ dB *above* that partial, so the leaked fundamental arrives only
$\sim12$ dB down — and it decays at $\sigma=b_1\approx0.19$ while the partial decays at
$\sigma\approx14$. Within a few hundred milliseconds the leak dominates and the estimator
returns $0.19$ for **every** high partial. Which is exactly what was observed, and it is not a
bug in the code so much as a failure to check the window's dynamic range against the signal's.

A 6th-order Butterworth, $|H|^2=[1+(f/f_c)^{12}]^{-1}$, rolls off at $-72$ dB/octave; the same
partial then measures $\sigma=12$–$16$, with a 15 kHz control band reading $-111$ dB. Use
`sosfiltfilt` for zero phase — but note it doubles the effective order **and smears energy
backwards in time**, so it is fine for decay and steady-state metrics and wrong for anything
measuring an attack.

### 9.2 Decay-rate regression and its censoring

Fit a line to the log envelope. With $\text{env}\propto e^{-\sigma t}$,

$$
20\log_{10}\text{env}=-\frac{20\sigma}{\ln 10}\,t+\text{const}
=-8.6859\,\sigma\,t+\text{const},
$$

hence `cal.py:17`'s division by $8.686$ to convert a dB/s slope into nepers/s.

**The censoring trap.** If the partial reaches a noise floor $n_0$ at $t_f$ and you fit over
$[0,T]$ with $T>t_f$, the regression returns approximately

$$
\hat\sigma\approx\frac{\ln(A_0/n_0)}{T},
$$

which does not depend on the true $\sigma$ at all — it measures how long the signal took to hit
the floor. This is why one 3.8 kHz partial read $\sigma\approx14$ over $[0.05,0.3]$ s and
$\sigma\approx2$ over $[0.05,1.2]$ s. Consistency check: $\ln(A_0/n_0)/1.2=2$ implies only 21 dB
of usable range, which is about right for that partial. **Always state the window with a
$\sigma$.**

### 9.3 Inharmonicity

Rearranging $\omega_n=\omega_1n\sqrt{1+Bn^2}$,

$$
\left(\frac{f_n}{n f_1}\right)^{2}-1=B\,n^2 ,
$$

so regress the left side on $n^2$; the slope is $B$ and the intercept should be zero. `cal.py`
does this iteratively, re-predicting each partial's search window from the current $B$ and
widening the tolerance to $\max(0.01f,\,0.5f_1,\,0.5|f_{\text{pred}}-nf_1|)$ so the search
cannot lose a partial it has mispredicted — necessary because $B$ and $f_1$ are correlated and
the iteration is a fixed point, not a one-shot fit.

### 9.4 Why the fixed-point calibration converges

`cal.py` runs multiplicative updates,

$$
\theta^{(k+1)}_j=\theta^{(k)}_j\cdot\operatorname{clip}\!\left(
\frac{y_j^{\text{target}}}{y_j(\theta^{(k)})},\ \ell_j,\ u_j\right),
$$

which is Newton's method in log-coordinates with the Jacobian approximated by the identity.
That approximation is legitimate here because the design is *deliberately* near-diagonal —
each parameter dominates one observable:

| parameter | observable | why it is nearly orthogonal to the others |
|---|---|---|
| $b_1$ | $\sigma_{\text{slow}}$ | measured on the fundamental, where $b_3\omega^2$ is negligible |
| `twinDamp` | $\sigma_{\text{fast}}/\sigma_{\text{slow}}$ | a ratio, so $b_1$ divides out |
| $b_3$ | $\sigma$ in 2–4 kHz | $b_3\omega^2\gg b_1$ up there |
| $B$ | comb stretch | frequencies, not amplitudes |

Convergence then requires only that the true log-Jacobian be diagonally dominant, and the
clips $[0.4,2.5]$, $[0.5,2.0]$ supply the damping. This is why it converges in six iterations
without a line search — and also why adding a parameter that touches two observables at once
would break it.

---

## 10. Error metrics, and a critique of each

`tools/evaluate_demo.py` measures four windowed chords across six bands.

**Band RMS.** $20\log_{10}\sqrt{\langle(\mathrm{BP}\,x)^2\rangle}$ over a 6th-order Butterworth
band. Honest, interpretable, and the workhorse. Its only real hazard is being applied to a band
where the reference is at its noise floor — see §11.

**Modulation depth.** Take the analytic signal $x_a=x+i\mathcal Hx$, envelope $|x_a|$, convert
to dB, remove a linear trend, take the standard deviation:

$$
M=\operatorname{std}_t\Big[20\log_{10}|x_a(t)|-\text{(linear fit)}\Big].
$$

Detrending removes the exponential decay, so $M$ measures wobble about the decay rather than
the decay itself, and it is invariant to both overall level and $\sigma$. A well-designed
metric. Reference reads 5.2–5.7 dB across bands; the model now matches within 0.3 dB, which is
how we know the flanging is genuinely fixed rather than merely quieter.

**Spectral flatness** (Wiener entropy):

$$
\mathrm{SFM}=\frac{\exp\!\big(\frac1K\sum_k\ln P_k\big)}{\frac1K\sum_kP_k}
=\frac{\text{geometric mean}}{\text{arithmetic mean}} .
$$

This one is a trap, and it should be removed or repaired. For a sum of $M$ sinusoids the
off-peak bins contain only numerical residue, so $\ln P_k\to-\infty$ and $\mathrm{SFM}\to0$ in
exact arithmetic; in practice the geometric mean is pinned by whatever the floor happens to be.
The model reads $-70$ dB, the reference $-39$ dB, and the $-31$ dB gap is essentially constant
across every chord — because it is measuring *the presence of a noise floor*, not timbre. The
only way to close it is to add noise.

If a flatness-like statistic is wanted, subtract the reference's own tail PSD first and compare
tonal residuals; otherwise delete it. Real broadband content below 8 kHz does exist — hammer,
damper and key noise — and it is already logged as the deferred mechanical-noise experiment.
That is the honest route, and it should stay deferred until the tonal part is finished.

---

## 11. Eight ways the measurements lied

Stated as statistical claims, because that is what they are.

1. **Spectral leakage.** A rectangular analysis window has $-13$ dB sidelobes and $-6$ dB/oct
   rolloff; against a 60 dB dynamic range it is useless. §9.1.
2. **Censored regression.** A decay fit run past the noise floor estimates
   $\ln(A_0/n_0)/T$, not $\sigma$. §9.2.
3. **Slice instead of profile.** Sweeping $p$ at clipped $K$ tests a curve through parameter
   space that avoids the optimum by construction. §6.1.
4. **Stale artefacts.** A `make` run whose output was grepped, so a failed rebuild was silent
   and all subsequent measurements described the previous binary.
5. **A parameter with one supporting datum.** `kPolarHz = 0.15` rested on a single clean
   measurement (D#1's fundamental) and beat notes the reference does not beat. It is now 0,
   documented as unconstrained.
6. **Fitting the recording's noise floor.** Two independent tests settle whether a band is real:

   *Attack versus tail.* Compare the band level in the attack against the same band in the
   file's own dead tail. At C4 the 8–12 kHz difference is $+0.4$ dB — the "content" is the
   room.

   *Peak-to-median ratio.* Periodogram bins of Gaussian noise are exponentially distributed,
   so the median is $\lambda\ln2$ and the maximum over $K$ bins has expectation
   $\approx\lambda\ln K$. Hence

   $$
   \mathrm{PMR}\approx10\log_{10}\frac{\ln K}{\ln 2}\;\;\xrightarrow{\;K\approx1500\;}\;\;10.2\ \mathrm{dB}.
   $$

   Measured: reference above 8 kHz, **10–15 dB** — Gaussian noise, as predicted to within a
   decibel. Model, same bands, **51–73 dB** — a sparse partial comb. The global EQ fitted to
   close that gap was chasing tape hiss, and it lifted 12–16 kHz to $+6.3$ dB above the
   reference. Both this and `bodyGain = 8` were produced by skipping this check.
7. **Extrapolation beyond the design region.** Every fitting tool in the repo used MIDI 27 and
   42 — both bass. Anchors above MIDI 48 were unconstrained by any audio, which `cp80_old`
   said in a comment that did not survive the rewrite. The mid-register brightness deficit of
   15–35 dB lived there undisturbed.
8. **A statistic invisible at $N=1$.** §8. Per-note validation cannot detect a chord-level
   summation error, and `bodyPolarity` passed every per-note test it had.

The through-line in 1, 2, 3, 6 and 7 is the same: **an estimator was trusted outside the regime
where it is consistent.** That is worth more than any individual fix.

---

## 12. References

**Strings and pianos**

- N. H. Fletcher and T. D. Rossing, *The Physics of Musical Instruments*, 2nd ed., Springer,
  1998. Ch. 2–3 for the stiff string; ch. 12 for the piano.
- A. Chaigne and A. Askenfelt, "Numerical simulations of piano strings. I. A physical model for
  a struck string using finite difference methods," *JASA* **95**(2), 1112–1118 (1994); "II.
  Comparisons with measurements and systematic exploration of some hammer-string parameters,"
  *JASA* **95**(3), 1631–1640 (1994). Source of the register-dependent $M_H/M_s$ ratios and the
  $\tau f_0$ trend used as a sanity check.
- G. Weinreich, "Coupled piano strings," *JASA* **62**(6), 1474–1484 (1977). §7.
- D. E. Hall, "Piano string excitation" I–VI, *JASA*, 1986–1992.
- H. A. Conklin, "Design and tone in the mechanoacoustic piano," *JASA*, 1996, three parts.
- C. Valette and C. Cuesta, *Mécanique de la corde vibrante*, Hermès, 1993. The three-term
  damping law of which §2 is the reduction.
- O. L. Railsback, "Scale temperament as applied to piano tuning," *JASA* **9**, 274 (1938).

**Hammers**

- X. Boutillon, "Model for piano hammers: Experimental determination and digital simulation,"
  *JASA* **83**, 746–754 (1988).
- A. Stulov, "Hysteretic model of the grand piano hammer felt," *JASA* **97**, 2577 (1995). The
  hereditary/Maxwell branch of §6.4.
- K. H. Hunt and F. R. E. Crossley, "Coefficient of restitution interpreted as damping in
  vibroimpact," *J. Appl. Mech.* **42**, 440–445 (1975).

**Signal processing**

- A. V. Oppenheim and R. W. Schafer, *Discrete-Time Signal Processing*. Impulse invariance,
  windows, halfband design.
- J. O. Smith III, *Physical Audio Signal Processing*, W3K, 2010. Scattering junctions,
  digital waveguides, and the modal/waveguide equivalence.
- L. Cohen, *Time-Frequency Analysis*, 1995, for the analytic signal and the caveats on
  envelope estimation.

**Instrument**

- Yamaha CP-80/CP-70B sales brochure ("ELECTRONICS", "CP-80/70B SPECIFICATIONS"): panel layout,
  tremolo with two out-of-phase outputs, detented tone controls, string disposition, key ranges.
- Yamaha CP-70B owner's manual, pp. 14–15: overall circuit diagram. The tone-stack RC values in
  the model are **plausible, not transcribed**, and replacing them is a transcription job rather
  than a modelling one.

---

## Appendix A. Symbols

| symbol | meaning | code |
|---|---|---|
| $\mu,\,m,\,m_1$ | linear density, total string mass, single-string mass | `mass`, `mass/nStrings` |
| $T$ | tension (§1–5); also sample period (§4) | — |
| $B$ | inharmonicity coefficient | `B` |
| $\omega_n,\,\sigma_n$ | modal frequency, modal decay rate (nepers/s) | `wN`, `sg` |
| $b_1,\,b_3$ | damping law coefficients | `b1`, `b3` |
| $x_0/L,\,w$ | strike position, contact width (both fractional) | `strike`, `hWidth` |
| $\phi_n$ | modal shape factor, comb $\times$ sinc | `sIn`, `shape` |
| $w_n$ | bridge readout weight, $(-1)^n n$ | `wOut` |
| $K,\,p,\,q$ | hammer stiffness, contact exponent, rate exponent | `hK`, `hP`, `kHammerRateQ` |
| $M_H,\,\eta,\,\tau$ | hammer mass, compression, contact time | `hM`, `eta`, `lastContactMs` |
| $Z$ | string characteristic impedance, $2m f_0$ | `waveZ` |
| $c$ | unison mistuning, cents | `unisonCents` |

## Appendix B. Constants worth remembering

$$
\frac{1200}{\ln 2}=1731.234\ \ (\text{cents per unit relative detuning}),\qquad
\frac{20}{\ln 10}=8.6859\ \ (\text{dB per neper})
$$

$$
\tfrac13B\!\left(\tfrac13,\tfrac12\right)=1.402\ \ (\text{contact-time prefactor at } p=2),
\qquad
10\log_{10}\frac{\ln K}{\ln 2}\approx10.2\ \mathrm{dB}\ \ (\text{PMR of Gaussian noise},\,K\approx1500)
$$
