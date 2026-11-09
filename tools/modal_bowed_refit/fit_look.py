"""Fit the engine's constants jointly to the picture score (look.py) over the
look_score.py notes, through the Python twin (Nelder-Mead).

fit_look.py [start.json] [out.json]: starts from sim.DEFAULT (or start.json),
writes the best so far to out.json (look_fit.json) as it goes.
"""
import json
import os
import sys
from multiprocessing import Pool

import numpy as np
from scipy.optimize import minimize

import look_score
import sim

NAMES = ["loss1", "bright", "lp_hz", "hiss", "rosin", "jitter", "rip_db", "rip_c", "slow_db", "loud_db", "direct_db", "rel_s"]
# FIT="rel_s,fade_db,ring": fit only these (the rest stay as they start)
NAMES = os.environ.get("FIT", ",".join(NAMES)).split(",")
LOG = {"loss1", "lp_hz", "hiss", "rosin", "rip_c", "rel_s", "ring", "ring_s"}
LO = dict(loss1=-6.5, bright=0, lp_hz=3.2, hiss=-4.5, rosin=-3, jitter=0, rip_db=0, rip_c=0.5, slow_db=0, loud_db=0, direct_db=-40, rel_s=-2.5, fade_db=0, ring=-1, ring_s=-3)
HI = dict(loss1=-4.3, bright=3, lp_hz=4.3, hiss=-2, rosin=0, jitter=0.25, rip_db=15, rip_c=2.0, slow_db=8, loud_db=5, direct_db=0, rel_s=0, fade_db=120, ring=2, ring_s=0.3)
STEP = dict(loss1=0.3, bright=0.7, lp_hz=0.15, hiss=0.4, rosin=0.5, jitter=0.03, rip_db=3, rip_c=0.3, slow_db=1.5, loud_db=1.0, direct_db=4, rel_s=0.25, fade_db=15, ring=0.4, ring_s=0.4)
START = dict(sim.DEFAULT)
if len(sys.argv) > 1:
    START.update(json.load(open(sys.argv[1])))
OUT = sys.argv[2] if len(sys.argv) > 2 else "look_fit.json"
FIXED = {k: v for k, v in START.items() if k in sim.DEFAULT and k not in NAMES}


def unpack(z):
    p = {}
    for k, v in zip(NAMES, z):
        v = min(max(v, LO[k]), HI[k])
        p[k] = 10 ** v if k in LOG else v
    return p


best = [1e9]


def f(z, pool):
    p = unpack(z)
    m, _ = look_score.score(dict(FIXED, **p), pool)
    if m["total"] < best[0]:
        best[0] = m["total"]
        json.dump(dict(FIXED, **p, score=m["total"]), open(OUT, "w"))
    print({k: round(float(v), 7) for k, v in p.items()}, {q: round(m[q], 2) for q in ("sustain", "wobble", "attack", "release", "plevel", "vibam", "slow", "total")}, flush=True)
    return m["total"]


if __name__ == "__main__":
    z0 = np.array([np.log10(START[k]) if k in LOG else START[k] for k in NAMES])
    simplex = [z0] + [z0 + np.eye(len(NAMES))[i] * STEP[k] for i, k in enumerate(NAMES)]
    with Pool(4) as pool:
        minimize(f, z0, args=(pool,), method="Nelder-Mead", options=dict(maxfev=200, initial_simplex=simplex, xatol=1e-3, fatol=0.02))
    print("BEST", best[0])
