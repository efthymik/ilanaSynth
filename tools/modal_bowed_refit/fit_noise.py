import importlib, numpy as np, json, sys
from scipy.optimize import minimize
t=importlib.import_module('score'); import sim, metrics as M
names=["jitter","hiss","hiss_hp"]
x0=np.array([0.03,0.003,6000.0]); lo=np.array([0.0,0.0,1500]); hi=np.array([0.3,0.05,12000])
def f(z):
    z=np.clip(z,lo,hi); p=dict(zip(names,z)); p["vib_ct"]=9
    err=[]
    for inst,n in t.SET:
        y=sim.play(inst,n,**p); c,h,b=t.REF[(inst,n)]
        err.append(np.mean(np.abs(M.hn(y,440*2**((n-69)/12))-h))+0.5*np.mean(np.abs(M.bands(y)-b)))
    e=float(np.mean(err)); print(np.round(z,4).tolist(),round(e,2),flush=True); return e
r=minimize(f,x0,method="Nelder-Mead",options=dict(maxfev=60,initial_simplex=[x0,x0*[1.6,1,1],x0*[1,2.5,1],x0*[1,1,0.6]]))
z=np.clip(r.x,lo,hi); json.dump(dict(zip(names,z.tolist())),open("noise_fit.json","w")); print("BEST",dict(zip(names,z.round(4))),r.fun)
