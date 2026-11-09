"""Score the Python twin on the picture score (look.py) over 8 violin and 8
cello notes. look_score.py ['{"param": value}']: run from the working folder
holding arco/<inst>/ (split.py). Spitfire's String Quartet pack is one
combined player (pitch picks the instrument), so its cello is only below G3.
"""
import json
import sys
from multiprocessing import Pool

import numpy as np
import soundfile as sf

import look
import sim

NAMES = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
VIOLIN = (55, 60, 64, 69, 74, 79, 84, 88)
CELLO = (36, 38, 41, 43, 45, 48, 50, 53)
SET = [("violin", n) for n in VIOLIN] + [("cello", n) for n in CELLO]


def ref(inst, n):
    name = NAMES[n % 12] + str(n // 12 - 1)
    x, _ = sf.read(f"arco/{inst}/r.{name}{name}.stereo.wav")
    return x.mean(1)[24000:24000 + int(4.8 * 48000)]  # (split.py puts 0.5 s before each note)


def one(args):
    inst, n, kw = args
    return look.dist(ref(inst, n), sim.play(inst, n, **kw), 440 * 2 ** ((n - 69) / 12))


def score(kw, pool, sets=SET):
    R = pool.map(one, [(i, n, kw) for i, n in sets])
    return {q: float(np.mean([r[q] for r in R])) for q in R[0]}, R


if __name__ == "__main__":
    kw = json.loads(sys.argv[1]) if len(sys.argv) > 1 else {}
    with Pool(4) as p:
        m, R = score(kw, p)
    for (i, n), r in zip(SET, R):
        print(i, n, {q: round(float(v), 1) for q, v in r.items()})
    print("MEAN", {q: round(v, 2) for q, v in m.items()})
