import numpy as np, soundfile as sf, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt, sys
from scipy.signal import spectrogram
NAMES=['C','Db','D','Eb','E','F','Gb','G','Ab','A','Bb','B']
eng_file=sys.argv[1] if len(sys.argv)>1 else "spit"
out=sys.argv[2] if len(sys.argv)>2 else "spec.png"
rows=[("violin",62),("violin",76),("cello",43),("cello",55)]
fig,ax=plt.subplots(len(rows),2,figsize=(14,3.2*len(rows)))
for r,(inst,n) in enumerate(rows):
    nm=NAMES[n%12]+str(n//12-1)
    x,sr=sf.read(f"arco/{inst}/r.{nm}{nm}.stereo.wav"); x=x.mean(1)[int(0.4*sr):int(5.2*sr)]
    i=[62,69,76,81].index(n) if inst=="violin" else [43,48,55,62].index(n)
    e,_=sf.read(f"{eng_file}-{inst}.wav"); e=e.mean(1); e=e[int(i*5*sr):int((i*5+4.8)*sr)]
    for c,(lab,y) in enumerate((("Spitfire",x),("engine",e))):
        f,t,S=spectrogram(y,sr,nperseg=2048,noverlap=1792)
        S=10*np.log10(S+1e-14); S-=S.max()
        ax[r,c].pcolormesh(t,f,S,vmin=-90,vmax=0,shading='auto',cmap='magma'); ax[r,c].set_yscale('symlog',linthresh=500); ax[r,c].set_ylim(50,16000)
        ax[r,c].set_title(f"{inst} {nm} {lab}")
plt.tight_layout(); plt.savefig(out,dpi=70)
