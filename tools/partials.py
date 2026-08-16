import subprocess, numpy as np, sys
sr=48000
def render(note,vel=0.8,secs=3.0):
    r=subprocess.run(['./build/analyze',str(note),str(vel),str(secs)],capture_output=True)
    return np.frombuffer(r.stdout,dtype=np.float32).astype(float)

def track(x,f0,nmax=40):
    seg=x[int(0.04*sr):int(1.2*sr)]
    X=np.abs(np.fft.rfft(seg*np.hanning(len(seg)),n=2**20)); f=np.fft.rfftfreq(2**20,1/sr)
    B=0.0
    for _ in range(6):                       # iterate: predict with current B, refit
        ns,fm,am=[],[],[]
        for n in range(1,nmax+1):
            g=f0*n*np.sqrt(1+B*n*n)
            if g>0.44*sr: break
            tol=max(0.004*g, 0.35*f0)        # window scales with the local spacing
            lo,hi=np.searchsorted(f,[g-tol,g+tol])
            if hi<=lo: break
            j=lo+np.argmax(X[lo:hi])
            if X[j]<X.max()*3e-5: continue
            ns.append(n); fm.append(f[j]); am.append(X[j])
        ns=np.array(ns,float);fm=np.array(fm);am=np.array(am)
        if len(ns)<6: break
        f1=fm[0]/np.sqrt(1+B)
        B=max(0.0,np.polyfit(ns**2,(fm/(ns*f1))**2-1,1)[0])
    return ns,fm,am,B

print(f"{'note':>5}{'f0':>8}{'B fit':>10}{'p10':>7}{'p20':>7}{'tilt dB/oct':>12}{'partials':>9}")
for note in [28,40,52,60,72,84]:
    x=render(note); f0=440*2**((note-69)/12)
    ns,fm,am,B=track(x,f0)
    f1=fm[0]
    p10=fm[list(ns).index(10)]/(10*f1) if 10 in ns else np.nan
    p20=fm[list(ns).index(20)]/(20*f1) if 20 in ns else np.nan
    tilt=np.polyfit(np.log2(fm),20*np.log10(am),1)[0]
    print(f"{note:5d}{f0:8.1f}{B:10.2e}{p10:7.3f}{p20:7.3f}{tilt:12.1f}{len(ns):9d}")
