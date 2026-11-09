import numpy as np
SR=48000
def track(x,f0,hop=240,n=4096):
    fr=np.fft.rfftfreq(1<<15,1/SR); out=[]
    for i in range(0,len(x)-n,hop):
        s=np.abs(np.fft.rfft(x[i:i+n]*np.hanning(n),1<<15)); best=None
        for m in (1,2,3):
            k=(fr>m*f0*0.95)&(fr<m*f0*1.05)
            j=np.argmax(s[k]); v=s[k][j]
            if 0<j<k.sum()-1:
                a,b,c=np.log(s[k][j-1:j+2]+1e-12); d=0.5*(a-c)/(a-2*b+c)
            else: d=0
            f=(fr[k][j]+d*(fr[1]-fr[0]))/m
            if best is None or v>best[0]: best=(v,f)
        out.append(best[1])
    return np.array(out), hop/SR
def vib(x,f0):
    t,dt=track(x[int(0.3*SR):int(3.8*SR)],f0); c=1200*np.log2(t/f0)
    w=int(0.6/dt); c=c-np.convolve(c,np.ones(w)/w,'same'); c=c[int(0.4/dt):-int(0.4/dt)]
    sp=np.abs(np.fft.rfft(c*np.hanning(len(c)),8192)); fr=np.fft.rfftfreq(8192,dt); k=(fr>3)&(fr<9)
    return fr[k][np.argmax(sp[k])], np.sqrt(2)*c.std()
def hn(x,f0):
    x=x[int(1.5*SR):int(1.5*SR)+8192]; n=8192; s=np.abs(np.fft.rfft(x*np.hanning(len(x)),n))**2; fr=np.fft.rfftfreq(n,1/SR)
    near=np.zeros(len(fr),bool)
    for m in range(1,int(16000/f0)): near|=np.abs(fr-m*f0)<max(f0*0.15,25)
    r=[]
    for lo,hi in ((100,1000),(1000,4000),(4000,16000)):
        b=(fr>lo)&(fr<hi); r.append(10*np.log10(s[near&b].sum()/(s[~near&b].sum()+1e-20)+1e-20))
    return np.array(r)
def envelope(x):
    e=np.sqrt(np.convolve(x**2,np.ones(480)/480,'same')); m=e[:int(3.9*SR)].max(); e=20*np.log10(e/m+1e-9)
    a20=np.argmax(e>-20)/SR; a3=np.argmax(e>-3)/SR
    st=e[int(2*SR):int(3.5*SR)].mean(); rel=e[int(4.0*SR):]; r40=np.argmax(rel<st-40)/SR
    return a20,a3,r40
def curve(x):
    """level (dB re sustain) at fixed times: the attack and release shape"""
    e=np.sqrt(np.convolve(x**2,np.ones(480)/480,'same')); st=np.sqrt(np.mean(x[int(2*SR):int(3.5*SR)]**2))
    T=[0.02,0.05,0.1,0.2,0.3,0.5,0.8,1.2,4.05,4.1,4.2,4.3,4.5]
    return np.array([20*np.log10(e[int(t*SR)]/st+1e-9) for t in T])
def bands(x, lo=1.5, hi=3.5):
    x=x[int(lo*SR):int(hi*SR)]; f=np.fft.rfftfreq(len(x),1/SR); p=np.abs(np.fft.rfft(x))**2
    E=[100,200,400,800,1600,3200,6400,12800,20000]
    v=np.array([10*np.log10(p[(f>=a)&(f<b)].sum()+1e-20) for a,b in zip(E,E[1:])]); return v-v.max()
