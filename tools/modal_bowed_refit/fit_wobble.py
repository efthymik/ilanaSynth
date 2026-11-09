"""Fit the bow's randomness: the picture score (look.py) plus the wobble measures
(wobble.py: each partial's skirt and fast flutter, the pitch's slow drift, the
vibrato cycle to cycle, the brightness's wobble, the noise under the partials), Spitfire's medians per instrument against the twin's.

FIT="fl_db,fl_hz,..." fit_wobble.py [start.json] [out.json]
"""
import json
import os
import sys
from multiprocessing import Pool

import numpy as np
from scipy.optimize import minimize

import look_score
import sim
import wobble

NAMES = os.environ.get("FIT", "fl_db,fl_hz,fl_slope,fl_hzslope,pdrift_ct,vib_depth_dev,vib_rate_dev,slow_db,rip_db,rip_c").split(",")
LOG = {"fl_hz", "pdrift_hz", "jit_hz", "rip_c"}
LO = dict(rip_c=0.5, fl_hzslope=-2, fl_db=0, fl_hz=0.3, fl_slope=-2, pdrift_ct=0, vib_depth_dev=0, vib_rate_dev=0, slow_db=0, rip_db=0, jit_ct=0, jit_hz=0.3)
HI = dict(rip_c=2.0, fl_hzslope=2, fl_db=6, fl_hz=1.6, fl_slope=2, pdrift_ct=10, vib_depth_dev=1.0, vib_rate_dev=0.5, slow_db=8, rip_db=15, jit_ct=15, jit_hz=1.6)
STEP = dict(rip_c=0.2, fl_hzslope=0.5, fl_db=1, fl_hz=0.3, fl_slope=0.7, pdrift_ct=2, vib_depth_dev=0.15, vib_rate_dev=0.05, slow_db=1, rip_db=2, jit_ct=2, jit_hz=0.3)
START = dict(sim.DEFAULT)
if len(sys.argv) > 1:
    START.update(json.load(open(sys.argv[1])))
OUT = sys.argv[2] if len(sys.argv) > 2 else "wobble_fit.json"
FIXED = {k: v for k, v in START.items() if k in sim.DEFAULT and k not in NAMES}


def ref_one(args):
    inst, n = args
    return wobble.measure(look_score.ref(inst, n), 440 * 2 ** ((n - 69) / 12))


def syn_one(args):
    inst, n, kw = args
    return wobble.measure(sim.play(inst, n, **kw), 440 * 2 ** ((n - 69) / 12))


def medians(R, inst):
    rows = [r for (i, _), r in zip(look_score.SET, R) if i == inst]
    return {k: float(np.nanmedian([r.get(k, np.nan) for r in rows])) for k in rows[0]}


def wobble_term(a, b):
    return (sum(abs(a["skirt_%d" % lo] - b["skirt_%d" % lo]) for lo in (10, 20, 40)) / 3
            + 2 * (abs(a["am_flut"] - b["am_flut"]) + abs(a["am_fast"] - b["am_fast"]))
            + 0.5 * abs(a["pitch_drift"] - b["pitch_drift"]) + 5 * abs(a["cyc_depth"] - b["cyc_depth"])
            + 5 * abs(a["cyc_rate"] - b["cyc_rate"])
            + 0.2 * (abs(a["bright_vib"] - b["bright_vib"]) + abs(a["bright_slow"] - b["bright_slow"]))
            + 0.3 * abs(a["noise_under"] - b["noise_under"]))


def unpack(z):
    p = {}
    for k, v in zip(NAMES, z):
        v = min(max(v, LO[k]), HI[k])
        p[k] = 10 ** v if k in LOG else v
    return p


best = [1e9]


def score(p, pool, REF):
    kw = dict(FIXED, **p)
    m, _ = look_score.score(kw, pool)
    S = pool.map(syn_one, [(i, n, kw) for i, n in look_score.SET])
    w = np.mean([wobble_term(REF[inst], medians(S, inst)) for inst in ("violin", "cello")])
    return m, w


def f(z, pool, REF):
    p = unpack(z)
    m, w = score(p, pool, REF)
    total = m["total"] + w
    if total < best[0]:
        best[0] = total
        json.dump(dict(FIXED, **p, score=total, look=m["total"], wobble=w), open(OUT, "w"))
    print({k: round(float(v), 4) for k, v in p.items()}, "look %.2f wobble %.2f total %.2f" % (m["total"], w, total), flush=True)
    return total


if __name__ == "__main__":
    with Pool(4) as pool:
        R = pool.map(ref_one, look_score.SET)
        REF = {inst: medians(R, inst) for inst in ("violin", "cello")}
        z0 = np.array([np.log10(START[k]) if k in LOG else START[k] for k in NAMES])
        simplex = [z0] + [z0 + np.eye(len(NAMES))[i] * STEP[k] for i, k in enumerate(NAMES)]
        minimize(f, z0, args=(pool, REF), method="Nelder-Mead", options=dict(maxfev=int(os.environ.get("MAXFEV", 150)), initial_simplex=simplex, xatol=1e-3, fatol=0.02))
    print("BEST", best[0])
