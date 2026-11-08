import numpy as np, soundfile as sf, sim, metrics as M, sys, json
NAMES=['C','Db','D','Eb','E','F','Gb','G','Ab','A','Bb','B']
T=np.array([0.02,0.04,0.07,0.1,0.15,0.2,0.3,0.4,0.6,0.8,1.0,4.05,4.1,4.15,4.2,4.3,4.4,4.5])
SR=48000
def curve(x):
    e=np.sqrt(np.convolve(x**2,np.ones(240)/240,'same')); st=np.sqrt(np.mean(x[int(2*SR):int(3.5*SR)]**2))
    return 20*np.log10(e[(T*SR).astype(int)]/st+1e-9)
SET=[("violin",55),("violin",64),("violin",72),("violin",81),("cello",38),("cello",45),("cello",52),("cello",60)]
REF={}
for inst,n in SET:
    nm=NAMES[n%12]+str(n//12-1); x,_=sf.read(f"arco/{inst}/r.{nm}{nm}.stereo.wav"); x=x.mean(1)[int(0.5*SR):int(5.3*SR)]
    REF[(inst,n)]=(curve(x),M.hn(x,440*2**((n-69)/12)),M.bands(x))
def score(p,verbose=False):
    tot=[]
    for inst,n in SET:
        y=sim.play(inst,n,**p); c,h,b=REF[(inst,n)]
        yc,yh,yb=curve(y),M.hn(y,440*2**((n-69)/12)),M.bands(y)
        ec=np.mean(np.abs(np.maximum(yc,-60)-np.maximum(c,-60))); eh=np.mean(np.abs(yh-h)); eb=np.mean(np.abs(yb[:7]-b[:7]))
        tot.append((ec,eh,eb))
        if verbose: print(inst,n,"curve %.1f hn %.1f bands %.1f"%(ec,eh,eb),"| hn",yh.round(0),"ref",h.round(0))
    t=np.mean(tot,0)
    if verbose: print("MEAN curve %.2f hn %.2f bands %.2f"%tuple(t))
    return t
if __name__=="__main__":
    p=json.loads(sys.argv[1]) if len(sys.argv)>1 else {}
    score(p,True)
