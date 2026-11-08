import numpy as np, soundfile as sf, glob, re, sys, json
NAMES={'C':0,'Db':1,'D':2,'Eb':3,'E':4,'F':5,'Gb':6,'G':7,'Ab':8,'A':9,'Bb':10,'B':11}
def note_of(p):
    m=re.search(r"r\.([A-G]b?)(\d)",p); return 12*(int(m.group(2))+1)+NAMES[m.group(1)]
def track(x,sr,f0,hop=240,n=4096):
    fr=np.fft.rfftfreq(1<<15,1/sr); out=[]
    for i in range(0,len(x)-n,hop):
        s=np.abs(np.fft.rfft(x[i:i+n]*np.hanning(n),1<<15))
        # track partial with best energy among 1..3 for robustness
        best=None
        for m in (1,2,3):
            k=(fr>m*f0*0.95)&(fr<m*f0*1.05)
            if not k.any(): continue
            j=np.argmax(s[k]); v=s[k][j]
            if j>0 and j<k.sum()-1:
                a,b,c=np.log(s[k][j-1:j+2]+1e-12); d=0.5*(a-c)/(a-2*b+c)
            else: d=0
            f=(fr[k][j]+d*(fr[1]-fr[0]))/m
            if best is None or v>best[0]: best=(v,f)
        out.append(best[1])
    return np.array(out), hop/sr
def hn(x,sr,f0):
    n=8192; s=np.abs(np.fft.rfft(x*np.hanning(len(x)),n))**2; fr=np.fft.rfftfreq(n,1/sr)
    near=np.zeros(len(fr),bool)
    for m in range(1,int(16000/f0)): near|=np.abs(fr-m*f0)<max(f0*0.15,25)
    res=[]
    for lo,hi in ((100,1000),(1000,4000),(4000,16000)):
        b=(fr>lo)&(fr<hi); res.append(10*np.log10(s[near&b].sum()/(s[~near&b].sum()+1e-20)+1e-20))
    return res
rows=[]
for inst in ("violin","viola","cello"):
    for p in sorted(glob.glob(f"arco/{inst}/*.wav")):
        n=note_of(p); f0=440*2**((n-69)/12)
        if f0>1300: continue
        x,sr=sf.read(p); x=x.mean(1)[int(0.5*sr):]
        t,dt=track(x[int(0.3*sr):int(3.8*sr)],sr,f0)
        c=1200*np.log2(t/f0); c-=np.convolve(c,np.ones(int(0.6/dt))/int(0.6/dt),'same')  # remove slow drift
        c=c[int(0.4/dt):-int(0.4/dt)]
        sp=np.abs(np.fft.rfft(c*np.hanning(len(c)),8192)); fr=np.fft.rfftfreq(8192,dt); k=(fr>3)&(fr<9)
        rate=fr[k][np.argmax(sp[k])]; depth=np.sqrt(2)*c.std()
        h=hn(x[int(1.5*sr):int(1.5*sr)+8192],sr,f0)
        e=np.sqrt(np.convolve(x**2,np.ones(480)/480,'same')); e=20*np.log10(e/e[:int(3.5*sr)].max()+1e-9)
        a20=np.argmax(e>-20)/sr; a3=np.argmax(e>-3)/sr
        rel=x[int(4.0*sr):]; # note off at 4 s
        er=20*np.log10(np.sqrt(np.convolve(rel**2,np.ones(480)/480,'same'))/e.max()+1e-9) if len(rel) else []
        st=20*np.log10(np.sqrt(np.mean(x[int(2*sr):int(3.5*sr)]**2)))
        r=np.sqrt(np.convolve(rel**2,np.ones(480)/480,'valid')); t40=np.argmax(20*np.log10(r+1e-12)<st-40)/sr if len(r) else 0
        rows.append(dict(inst=inst,note=n,f0=round(f0,1),rate=round(rate,2),depth=round(depth,1),hn=[round(v,1) for v in h],a20=round(a20,3),a3=round(a3,3),release40=round(t40,2)))
json.dump(rows,open("measure.json","w"))
import collections
for inst in ("violin","viola","cello"):
    R=[r for r in rows if r['inst']==inst]
    print(inst,len(R),"rate %.2f"%np.median([r['rate'] for r in R]),"depth %.0f ct"%np.median([r['depth'] for r in R]),
          "hn",np.median([r['hn'] for r in R],0).round(1),"a20 %.2f a3 %.2f"%(np.median([r['a20'] for r in R]),np.median([r['a3'] for r in R])),"rel40 %.2f"%np.median([r['release40'] for r in R]))
    for r in R[::5]: print("  ",r)
