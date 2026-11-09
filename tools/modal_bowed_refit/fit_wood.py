"""Fit the wood: the picture score (look.py), the wobble measures (wobble.py)
and the wood measures (wood.py: the floor between and under the partials, each
partial's flutter by octave), Spitfire against the twin, each note rendered once.

FIT="fl_db,fl_knee,..." fit_wood.py [start.json] [out.json]
"""
import json
import os
import sys
from multiprocessing import Pool

import numpy as np
from scipy.optimize import minimize

import look
import look_score
import sim
import wobble
import wood
from fit_wobble import medians, wobble_term

NAMES = os.environ.get("FIT", "fl_db,fl_knee,fl_fslope,rip_db,rip_knee,rip_fslope,direct_hp,rosin,jitter").split(",")
LOG = {"fl_knee", "rip_knee", "direct_hp", "rosin", "fl_hz", "jitter"}
LO = dict(jitter=-2.0, fl_db=0, fl_knee=2.3, fl_fslope=0, rip_db=0, rip_knee=2.0, rip_fslope=0, direct_hp=1.3, rosin=-2.5, fl_hz=0.3, fl_hzslope=-2)
HI = dict(jitter=-0.3, fl_db=60, fl_knee=3.9, fl_fslope=3, rip_db=15, rip_knee=3.6, rip_fslope=3, direct_hp=2.7, rosin=1.0, fl_hz=1.6, fl_hzslope=2)
STEP = dict(jitter=0.3, fl_db=3, fl_knee=0.2, fl_fslope=0.4, rip_db=2, rip_knee=0.3, rip_fslope=0.5, direct_hp=0.3, rosin=0.5, fl_hz=0.2, fl_hzslope=0.4)
START = dict(sim.DEFAULT, fl_db=3.0, fl_slope=0.0, fl_knee=1500.0, fl_fslope=1.0, rip_knee=600.0, rip_fslope=1.0, direct_hp=150.0)
if len(sys.argv) > 1:
    START.update(json.load(open(sys.argv[1])))
OUT = sys.argv[2] if len(sys.argv) > 2 else "wood_fit.json"
FIXED = {k: v for k, v in START.items() if k in sim.DEFAULT and k not in NAMES}


def ref_one(args):
    inst, n = args
    f0 = 440 * 2 ** ((n - 69) / 12)
    x = look_score.ref(inst, n)
    w, rows = wobble.measure(x, f0, rows=True)
    return x, w, wood.measure(x, f0, rows)


def syn_one(args):
    inst, n, kw, x = args
    f0 = 440 * 2 ** ((n - 69) / 12)
    y = sim.play(inst, n, **kw)
    w, rows = wobble.measure(y, f0, rows=True)
    return look.dist(x, y, f0), w, wood.measure(y, f0, rows)


def unpack(z):
    p = {}
    for k, v in zip(NAMES, z):
        v = min(max(v, LO[k]), HI[k])
        p[k] = 10 ** v if k in LOG else v
    return p


best = [1e9]


def f(z, pool, REF):
    p = unpack(z)
    kw = dict(FIXED, **p)
    S = pool.map(syn_one, [(i, n, kw, REF["x"][j]) for j, (i, n) in enumerate(look_score.SET)])
    lk = float(np.mean([s[0]["total"] for s in S]))
    wb = np.mean([wobble_term(REF["wob"][inst], medians([s[1] for s in S], inst)) for inst in ("violin", "cello")])
    wd = np.mean([wood.term(REF["wood"][inst], wood.summary([s[2] for (i, _), s in zip(look_score.SET, S) if i == inst]))
                  for inst in ("violin", "cello")])
    total = lk + wb + wd
    if total < best[0]:
        best[0] = total
        json.dump(dict(FIXED, **p, score=total, look=lk, wobble=float(wb), wood=float(wd)), open(OUT, "w"))
    print({k: round(float(v), 4) for k, v in p.items()}, "look %.2f wobble %.2f wood %.2f total %.2f" % (lk, wb, wd, total), flush=True)
    return total


if __name__ == "__main__":
    with Pool(4) as pool:
        R = pool.map(ref_one, look_score.SET)
        REF = dict(x=[r[0] for r in R],
                   wob={inst: medians([r[1] for r in R], inst) for inst in ("violin", "cello")},
                   wood={inst: wood.summary([r[2] for (i, _), r in zip(look_score.SET, R) if i == inst]) for inst in ("violin", "cello")})
        z0 = np.array([np.log10(START[k]) if k in LOG else START[k] for k in NAMES])
        simplex = [z0] + [z0 + np.eye(len(NAMES))[i] * STEP[k] for i, k in enumerate(NAMES)]
        minimize(f, z0, args=(pool, REF), method="Nelder-Mead", options=dict(maxfev=int(os.environ.get("MAXFEV", 200)), initial_simplex=simplex, xatol=1e-3, fatol=0.02))
    print("BEST", best[0])
