import os, soundfile as sf, numpy as np
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt

def load(p):
    x,sr=sf.read(p); x=np.asarray(x,float)
    return (x.mean(1) if x.ndim>1 else x), sr

def peak(X,f,j):                      # parabolic interpolation for sub-bin accuracy
    if j<=0 or j>=len(X)-1: return f[j],X[j]
    a,b,c=np.log(X[j-1]+1e-30),np.log(X[j]+1e-30),np.log(X[j+1]+1e-30)
    d=0.5*(a-c)/(a-2*b+c+1e-30); d=np.clip(d,-1,1)
    return f[j]+d*(f[1]-f[0]), np.exp(b-0.25*(a-c)*d)

def track(x,sr,f0,nmax=40,t0=0.03,t1=2.0):
    seg=x[int(t0*sr):int(t1*sr)]
    N=2**21
    X=np.abs(np.fft.rfft(seg*np.hanning(len(seg)),n=N)); f=np.fft.rfftfreq(N,1/sr)
    # fundamental: widest search, then refine
    lo,hi=np.searchsorted(f,[f0*0.94,f0*1.06]); j=lo+np.argmax(X[lo:hi])
    f1,a1=peak(X,f,j)
    ns,fm,am=[1.0],[f1],[a1]
    B=0.0
    for n in range(2,nmax+1):
        g=f1*n*np.sqrt(1+B*n*n)
        if g>0.45*sr: break
        # window scales with how uncertain the stretch is at this n
        tol=max(0.010*g, 0.5*f1, 0.5*abs(g-f1*n))
        lo,hi=np.searchsorted(f,[g-tol,g+tol])
        if hi<=lo+2: break
        j=lo+np.argmax(X[lo:hi]); fp,ap=peak(X,f,j)
        if ap<X.max()*1e-4: continue
        ns.append(float(n)); fm.append(fp); am.append(ap)
        if len(ns)>=4:                              # refit B on everything so far
            A=np.array(ns); F=np.array(fm)
            B=max(0.0,np.polyfit(A**2,(F/(A*f1))**2-1,1)[0])
    return np.array(ns),np.array(fm),np.array(am),B,f1

def env_at(x,sr,fc,bw=8.0):
    t=np.arange(len(x))/sr
    z=x*np.exp(-2j*np.pi*fc*t)
    k=int(sr/bw); k+=(k+1)%2; w=np.hanning(k); w/=w.sum()
    e=np.abs(np.convolve(z,w,'same'))
    return t,20*np.log10(e/e.max()+1e-12)

REF_DIR=os.environ.get('CP80_REFERENCE_DIR','reference')
MODEL_DIR=os.environ.get('CP80_MODEL_DIR','out/model-lib')
pairs=[(27,os.path.join(REF_DIR,'027-D#1-F.flac'),os.path.join(MODEL_DIR,'027-D#1-F.flac'),'D#1'),
       (42,os.path.join(REF_DIR,'042-F#2-F.flac'),os.path.join(MODEL_DIR,'042-F#2-F.flac'),'F#2')]

fig,axes=plt.subplots(2,2,figsize=(13,9))
print(f"{'note':<6}{'src':<7}{'f1 Hz':>9}{'cents':>7}{'B':>10}{'p10':>7}{'tilt':>8}{'@4s':>7}{'@8s':>7}")
store={}
for row,(midi,refp,modp,label) in enumerate(pairs):
    f0=440*2**((midi-69)/12); res={}
    for tag,path in (('ref',refp),('model',modp)):
        x,sr=load(path); ns,fm,am,B,f1=track(x,sr,f0)
        adb=20*np.log10(am/am[0])
        tilt=np.polyfit(np.log2(fm),adb,1)[0]
        t,e=env_at(x,sr,f1)
        d4=e[min(int(4*sr),len(e)-1)]; d8=e[min(int(8*sr),len(e)-1)]
        p10=fm[list(ns).index(10.0)]/(10*f1) if 10.0 in ns else np.nan
        res[tag]=(ns,fm,adb,B,f1,t,e,tilt)
        print(f"{label:<6}{tag:<7}{f1:9.2f}{1200*np.log2(f1/f0):7.0f}{B:10.2e}{p10:7.3f}{tilt:8.1f}{d4:7.1f}{d8:7.1f}")
    store[label]=res
    ax=axes[row,0]
    for tag,c,m in (('ref','#111','o'),('model','crimson','s')):
        ns,fm,adb,B,f1,t,e,tilt=res[tag]
        ax.plot(ns,adb,marker=m,color=c,ms=4,lw=1.3,label=f"{tag}: B={B:.1e}, tilt={tilt:.1f} dB/oct")
    ax.set_xlabel('partial n'); ax.set_ylabel('dB rel. fundamental')
    ax.set_title(f'{label} F — partial amplitude envelope'); ax.grid(alpha=.3); ax.legend(fontsize=8); ax.set_ylim(-85,10)
    ax=axes[row,1]
    for tag,c in (('ref','#111'),('model','crimson')):
        ns,fm,adb,B,f1,t,e,tilt=res[tag]
        ax.plot(t,e,color=c,lw=1.0,label=tag)
    ax.set_xlabel('time (s)'); ax.set_ylabel('fundamental dB'); ax.set_xlim(0,10); ax.set_ylim(-80,5)
    ax.set_title(f'{label} F — fundamental decay'); ax.grid(alpha=.3); ax.legend(fontsize=8)
plt.tight_layout(); plt.savefig('compare.png',dpi=130)

# difference curve: the diagnostic that matters
fig2,ax=plt.subplots(1,2,figsize=(12,4.2))
for i,label in enumerate(store):
    r=store[label]['ref']; m=store[label]['model']
    common=sorted(set(r[0]).intersection(set(m[0])))
    rr={n:a for n,a in zip(r[0],r[2])}; mm={n:a for n,a in zip(m[0],m[2])}
    d=[mm[n]-rr[n] for n in common]
    ax[i].axhline(0,color='k',lw=.8)
    ax[i].plot(common,d,'o-',color='#1a6',ms=4)
    sl=np.polyfit(np.log2([r[1][list(r[0]).index(n)] for n in common]),d,1)[0]
    ax[i].set_title(f'{label} — model minus reference   (slope {sl:+.1f} dB/oct)')
    ax[i].set_xlabel('partial n'); ax[i].set_ylabel('model - ref (dB)'); ax[i].grid(alpha=.3)
plt.tight_layout(); plt.savefig('residual.png',dpi=130)
print('\nsaved compare.png, residual.png')
