#!/usr/bin/env python3
"""Render a sparse sample library, matching reference-library naming.

    NNN-NOTE-VEL.flac    NNN = MIDI note (60 = C4), VEL = PP/MP/F/FF

ONE global gain across the whole set, so relative level between velocity layers and
between registers survives. Never normalise per file -- that destroys the comparison.
"""
import subprocess, numpy as np, soundfile as sf, os, sys
from concurrent.futures import ThreadPoolExecutor
SR=48000; SECS=12.5
NAMES=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
def nm(m): return f"{NAMES[m%12]}{m//12-1}"
VELS=[('PP',25.0/127.0),('MP',65.5/127.0),('F',92.5/127.0),('FF',115.5/127.0)]
NOTES=[27,34,42,46,50,62,74]     # 27/34 single-strung, 42 = boundary, rest bichord
OUT=sys.argv[1] if len(sys.argv)>1 else 'out/model-lib'
os.makedirs(OUT,exist_ok=True)
R={}
jobs=[(m,tag,v) for m in NOTES for tag,v in VELS]
def render(job):
    m,tag,v=job
    r=subprocess.run(['./build/analyze',str(m),str(v),str(SECS)],capture_output=True,check=True)
    return (m,tag),np.frombuffer(r.stdout,dtype=np.float32).astype(np.float64)
with ThreadPoolExecutor(max_workers=min(8,os.cpu_count() or 1)) as pool:
    for key,x in pool.map(render,jobs): R[key]=x
g=0.89/max(np.abs(x).max() for x in R.values())
for (m,tag),x in R.items():
    sf.write(f"{OUT}/{m:03d}-{nm(m)}-{tag}.flac",(x*g)[:int(12.0*SR)].astype(np.float32),SR,subtype='PCM_24')
print(f"wrote {len(R)} files to {OUT} (global gain {g:.1f})")
