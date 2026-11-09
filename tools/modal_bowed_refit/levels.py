"""Each note's loudness, the twin against Spitfire (dB, mean removed): levels.py [params.json]."""
import json
import sys
from multiprocessing import Pool

import numpy as np
import soundfile as sf

import sim

NAMES = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
kw = {k: v for k, v in (json.load(open(sys.argv[1])).items() if len(sys.argv) > 1 else []) if k in sim.DEFAULT}


def rms(x):
    return 20 * np.log10(np.sqrt((x[48000 * 2:48000 * 4] ** 2).mean()))


def one(a):
    inst, n = a
    name = NAMES[n % 12] + str(n // 12 - 1)
    x, _ = sf.read(f"arco/{inst}/r.{name}{name}.stereo.wav")
    return rms(x.mean(1)[24000:]), rms(sim.play(inst, n, **kw))


if __name__ == "__main__":
    for inst, notes in (("violin", range(55, 90, 2)), ("cello", range(36, 55, 2))):
        with Pool(4) as p:
            R = np.array(p.map(one, [(inst, n) for n in notes]))
        d = R[:, 1] - R[:, 0]
        d -= d.mean()
        print(inst, " ".join("%d:%+.0f" % (n, v) for n, v in zip(notes, d)), "spread %.1f dB" % d.std())
