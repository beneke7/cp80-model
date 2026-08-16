import os, re, subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf
from spectral_balance import band_sigmas
def load(p):
    x,sr=sf.read(p); x=np.asarray(x,float); return (x.mean(1) if x.ndim>1 else x),sr
def upper_env(x,sr,fc,hop=0.05,win=0.35):
    """peak-following envelope of one partial: immune to beating nulls"""
    t=np.arange(len(x))/sr; z=x*np.exp(-2j*np.pi*fc*t)
    k=int(win*sr); h=int(hop*sr)
    c=np.concatenate([[0],np.cumsum(np.abs(z)**2)])
    n=(len(z)-k)//h
    tt=np.array([(i*h+k/2)/sr for i in range(n)])
    e=np.array([np.sqrt((c[i*h+k]-c[i*h])/k) for i in range(n)])
    return tt,20*np.log10(e/e.max()+1e-12)
def slope(tt,e,a,b):
    m=(tt>=a)&(tt<=b)&(e>-70)
    return -np.polyfit(tt[m],e[m],1)[0]/8.686 if m.sum()>4 else np.nan   # -> sigma (1/s)
def tiltof(x,sr,f0,N=2**20):
    seg=x[int(0.03*sr):int(1.5*sr)]
    X=np.abs(np.fft.rfft(seg*np.hanning(len(seg)),n=N)); f=np.fft.rfftfreq(N,1/sr)
    lo,hi=np.searchsorted(f,[f0*.94,f0*1.06]); j=lo+np.argmax(X[lo:hi]); f1=f[j]
    B=0.0; fm=[f1]; am=[X[j]]; ns=[1.0]
    for n in range(2,33):
        g=f1*n*np.sqrt(1+B*n*n)
        if g>0.45*sr: break
        tol=max(0.010*g,0.5*f1,0.5*abs(g-f1*n)); a,b=np.searchsorted(f,[g-tol,g+tol])
        if b<=a+2: break
        j=a+np.argmax(X[a:b])
        if X[j]<X.max()*1e-4: continue
        ns.append(float(n)); fm.append(f[j]); am.append(X[j])
        if len(ns)>=5:
            A=np.array(ns);F=np.array(fm); B=max(0.0,np.polyfit(A**2,(F/(A*f1))**2-1,1)[0])
    am=np.array(am); fm=np.array(fm)
    return np.polyfit(np.log2(fm),20*np.log10(am/am[0]),1)[0], B, f1

REF_DIR=os.environ.get('CP80_REFERENCE_DIR','../GregSullivan.E-Pianos/CP80/Samples')
NAMES=('C','C#','D','D#','E','F','F#','G','G#','A','A#','B')
def note_name(note):
    return f'{NAMES[note%12]}{note//12-1}'

# One measured forte file per anchor region. Intermediate registers are real recordings,
# not extrapolated bass targets.
FIT=((0,27),(1,42),(2,50),(3,60),(4,72),(5,85),(6,107))
REF={m:os.path.join(REF_DIR,f'{m:03d}-{note_name(m)}-F.flac') for _,m in FIT}
tgt={}
for m,p in REF.items():
    x,sr=load(p); f0=440*2**((m-69)/12); ti,B,f1=tiltof(x,sr,f0)
    tt,e=upper_env(x,sr,f1)
    sigmas=band_sigmas(x,sr)
    tgt[m]=dict(sig_slow=slope(tt,e,1.5,8.0), sig_fast=slope(tt,e,0.05,0.8),
                tilt=ti, B=B, f1=f1,
                sigma_2k4k=float(np.nanmedian(sigmas[2:4])))
    d=tgt[m]; print(f"ref {m}: sigma_slow={d['sig_slow']:.3f}  sigma_fast={d['sig_fast']:.3f}  ratio={d['sig_fast']/d['sig_slow']:.1f}  tilt={ti:.1f}  B={B:.2e}")

def load_anchors():
    """Read kAnchors so the fitter cannot silently drift from the engine."""
    source = Path(__file__).resolve().parents[1] / "src" / "cp80.hpp"
    rows = []
    for line in source.read_text().splitlines():
        values = re.findall(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?", line.split("//", 1)[0])
        if len(values) != 14 or "f" not in line or not line.lstrip().startswith("{"):
            continue
        rows.append([len(rows)] + [float(value) for value in values[1:]])
    if len(rows) != 7:
        raise SystemExit(f"expected 7 kAnchors in {source}, found {len(rows)}")
    return rows


# Current source anchors, in the same order as kAnchors.
ANCH = load_anchors()
def wr():
    with open('/tmp/a.txt','w') as f:
        for q in ANCH: f.write(" ".join(f"{z:.6g}" for z in q)+"\n")
def render(n):
    e=dict(os.environ); e['CP80_ANCHORS']='/tmp/a.txt'
    r=subprocess.run(['./build/analyze',str(n),'0.7283464567','9.0'],capture_output=True,env=e,check=True)
    return np.frombuffer(r.stdout,dtype=np.float32).astype(np.float64),48000
def meas(m):
    x,sr=render(m); f0=440*2**((m-69)/12); ti,B,f1=tiltof(x,sr,f0)
    tt,e=upper_env(x,sr,f1)
    sigmas=band_sigmas(x,sr)
    return dict(sig_slow=slope(tt,e,1.5,8.0), sig_fast=slope(tt,e,0.05,0.8),
                tilt=ti, B=B, sigma_2k4k=float(np.nanmedian(sigmas[2:4])))

# fixed-point calibration: each parameter drives one measurement almost independently
print("\nfixed-point calibration")
for it in range(6):
    wr()
    with ThreadPoolExecutor(max_workers=len(FIT)) as pool:
        measured=dict(zip((m for _,m in FIT), pool.map(meas, (m for _,m in FIT))))
    # b1 <- slow decay at each anchor's measured register.
    for idx,mm in FIT:
        rr=measured[mm]
        if np.isfinite(rr['sig_slow']) and np.isfinite(tgt[mm]['sig_slow']):
            k=np.clip(tgt[mm]['sig_slow']/max(rr['sig_slow'],1e-3),0.4,2.5)
            ANCH[idx][3]*=k
    # twinDamp <- fast/slow ratio
    for idx,mm in FIT:
        rr=measured[mm]
        got=rr['sig_fast']/max(rr['sig_slow'],1e-3); want=tgt[mm]['sig_fast']/tgt[mm]['sig_slow']
        if np.isfinite(got) and np.isfinite(want):
            ANCH[idx][12]=float(np.clip(ANCH[idx][12]*np.clip(want/max(got,1e-2),0.5,2.0),1.05,9.0))
    # hW is a physical contact width, not a proxy for a bad decay metric.
    for q in ANCH: q[11]=float(np.clip(q[11],0.002,0.02))
    # b3 <- the measured 2--4 kHz amplitude decay; b1 remains the slow-tail fit.
    for idx,mm in FIT:
        rr=measured[mm]
        target_sigma=float(np.nanmedian([tgt[mm].get('sigma_2k4k', np.nan)]))
        if np.isfinite(target_sigma) and np.isfinite(rr['sigma_2k4k']):
            ANCH[idx][4]*=float(np.clip((target_sigma-tgt[mm]['sig_slow']) /
                                         max(rr['sigma_2k4k']-rr['sig_slow'],1e-3),0.5,2.0))
    # B <- measured inharmonicity
    for idx,mm in FIT:
        rr=measured[mm]
        if tgt[mm]['B'] > 0.0 and rr['B'] > 0.0 and np.isfinite(rr['B']):
            ANCH[idx][1]*=float(np.clip(tgt[mm]['B']/rr['B'],0.5,2.0))
    print("  it%d:" % it, " | ".join(
        f"n{mm} sig={measured[mm]['sig_slow']:.3f}/{tgt[mm]['sig_slow']:.3f} "
        f"B={measured[mm]['B']:.2e}/{tgt[mm]['B']:.2e}" for _,mm in FIT))
wr()
print("\nfitted anchors (idx B strike b1 b3 ... hW twin slowW):")
for q in ANCH: print("  ",[f"{z:.4g}" for z in [q[1],q[3],q[11],q[12]]])
np.save('/tmp/anch.npy',np.array(ANCH))
