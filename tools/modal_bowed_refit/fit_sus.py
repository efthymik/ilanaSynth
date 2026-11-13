"""Body gains from the held tone's average band shape (look.py's sustain bands),
over every Spitfire note in range except look_score.SET (held out for scoring)."""
import sys, json, os, numpy as np, soundfile as sf, look, look_score, sim
from multiprocessing import Pool
SR=48000
NAMES=["C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"]
RANGE={"violin":(55,100),"cello":(36,55)}
f=np.fft.rfftfreq(2048,1/SR)
FC=np.array([np.sqrt(a*b) for a,b in zip(look.EDGES,look.EDGES[1:]) if ((f>=a)&(f<b)).any()])
def exists(inst,n):
    nm=NAMES[n%12]+str(n//12-1); return os.path.exists(f"arco/{inst}/r.{nm}{nm}.stereo.wav")
def one(a):
    inst,n,kw,gains=a
    sim.BODY[inst]=np.array(gains)
    x=look_score.ref(inst,n); y=sim.play(inst,n,**kw)
    fl=lambda v: np.maximum(v,-70)
    return fl(look.band_parts(x)["sustain"])-fl(look.band_parts(y)["sustain"])
if __name__=="__main__":
    kw=json.loads(open(sys.argv[1]).read()) if len(sys.argv)>1 else {}
    held=set(look_score.SET)
    if os.environ.get('START'):
        for k,v in json.load(open(os.environ['START'])).items(): sim.BODY[k]=np.array(v['gains'])
    out={}
    with Pool(2) as p:
        for inst in ("violin","cello"):
            notes=[n for n in range(*RANGE[inst]) if exists(inst,n) and (inst,n) not in held]
            gains=sim.BODY[inst].copy(); offset=gains.max(); centres=sim.BODY["hz"]/sim.SIZES[inst]
            if inst=='violin' and os.environ.get('NO185'): gains[0]=-40+offset-0*gains.max()
            best=(1e9,gains.copy())
            for step in range(8):
                D=np.array(p.map(one,[(inst,n,kw,gains.tolist()) for n in notes]))
                m=D.mean(0); e=np.abs(D).mean()
                print(inst,step,"mean|d| %.3f systematic %.3f"%(e,np.abs(m).mean()),flush=True)
                if e<best[0]: best=(e,gains.copy())
                corr=np.interp(np.log2(centres),np.log2(FC),m)
                gains=gains+0.5*corr; gains-=gains.max(); gains=np.maximum(gains,-40)+offset
            out[inst]=dict(err=best[0],gains=best[1].tolist())
            print(inst,"best",round(best[0],3),flush=True)
    json.dump(out,open(os.environ.get("OUT","sus_bodies2.json"),"w"))
