import sys, json, numpy as np
sys.path.insert(0,__import__("os").path.join(__import__("os").path.dirname(__file__),"..")); sys.argv=sys.argv[:2]
import fit_modal_bowed_body as F, sim
inst=sys.argv[1]
P={}  # the engine as fitted (sim.DEFAULT)
D="arco/"+inst
notes=F.notes_in(D); notes=notes[::max(1,len(notes)//16)][:16]
import re
size=sim.SIZES[inst]; centres=sim.BODY["hz"]/size
gains=sim.BODY[inst].copy(); best=(1e9,gains.copy())
for step in range(7):
    sim.BODY[inst]=gains
    corr=[[] for _ in gains]; errs=[]
    for i,(rec,f0) in enumerate(notes):
        note=int(round(69+12*np.log2(f0/440)))
        y=sim.play(inst,note,**P)[int(1.4*48000):int(3.9*48000)]
        pr=F.partial_levels(rec,f0); pm=F.partial_levels(y,f0)
        c=min(len(pr),len(pm)); d=pr[:c,1]-pm[:c,1]; d-=d.mean(); errs.append(np.mean(np.abs(d)))
        for f,dv in zip(pr[:c,0],d): corr[int(np.argmin(np.abs(np.log(centres/f))))].append(dv)
    e=np.mean(errs); print(inst,step,round(e,2),flush=True)
    if e<best[0]: best=(e,gains.copy())
    gains=gains+0.5*np.array([np.mean(c) if c else 0 for c in corr]); gains-=gains.max(); gains=np.maximum(gains,-40)
json.dump(dict(err=best[0],gains=best[1].tolist()),open(f"body2-{inst}.json","w"))
