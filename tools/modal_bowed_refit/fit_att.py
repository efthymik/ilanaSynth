import importlib, numpy as np, json, sys
from scipy.optimize import minimize
sys.argv=['x']; t=importlib.import_module('score'); import sim
t.SET=[("violin",55),("violin",64),("violin",72),("violin",81),("cello",38),("cello",45),("cello",52),("cello",60)]
base=dict(rel_s=0.002,fade_db=0,vib_ct=9,jitter=0.0905,hiss=0.0052,hiss_hp=8000)
names=["attack_scale","swell_s","swell_amt"]; lo=np.array([0.05,0.05,0.0]); hi=np.array([6,3,0.97])
A=t.T<=1.0
def f(z):
    z=np.clip(z,lo,hi); p=dict(base); p.update(zip(names,z)); err=[]
    for inst,n in t.SET:
        y=sim.play(inst,n,**p); c=t.REF[(inst,n)][0]
        err.append(np.mean(np.abs(np.maximum(t.curve(y),-60)-np.maximum(c,-60))[A]))
    e=float(np.mean(err)); print(np.round(z,3).tolist(),round(e,2),flush=True); return e
x0=np.array([0.5,0.5,0.85])
r=minimize(f,x0,method="Nelder-Mead",options=dict(maxfev=50,initial_simplex=[x0,x0*[3,1,1],x0*[1,2,1],x0*[1,1,0.8]]))
z=np.clip(r.x,lo,hi); json.dump(dict(zip(names,z.tolist())),open("att_fit.json","w")); print("BEST",z.round(3),r.fun)
