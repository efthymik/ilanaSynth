"""Refit one instrument's body gains through the whole engine (the Python twin).

fit_body2.py <violin|viola|cello> [params.json]: run from the working folder
holding arco/<inst>/ (split.py). Writes body2-<inst>.json with the gains in
the header's absolute dB (the instrument's loudness offset kept), ready for
write_bodies.py. Besides each note's partial shape it fits each note's
loudness against the recording's (FIT_LEVEL, 0.5), so no note jumps out. Spitfire's String Quartet pack is one combined player
(pitch picks the instrument): its "cello" render is a cello only below G3,
its violin render a violin from G3 up, so the notes are limited to those.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import fit_modal_bowed_body as F  # noqa: E402
import sim  # noqa: E402

inst = sys.argv[1]
P = {k: v for k, v in (json.load(open(sys.argv[2])).items() if len(sys.argv) > 2 else []) if k in sim.DEFAULT}
D = os.path.abspath("arco/" + inst)
LEVEL = float(os.environ.get("FIT_LEVEL", "0.5"))  # weight of each note's loudness against the recording's
RANGE = {"cello": (60.0, 195.0), "violin": (195.0, 2000.0), "viola": (125.0, 1000.0)}[inst]
notes = [(rec, f0) for rec, f0 in F.notes_in(D) if RANGE[0] <= f0 < RANGE[1]]
notes = notes[::max(1, len(notes) // 16)][:16]
size = sim.SIZES[inst]
centres = sim.BODY["hz"] / size
gains = sim.BODY[inst].copy()
offset = gains.max()  # the instrument's loudness offset: kept
best = (1e9, gains.copy())
for step in range(7):
    sim.BODY[inst] = gains
    corr = [[] for _ in gains]
    errs = []
    diffs = []
    for rec, f0 in notes:
        note = int(round(69 + 12 * np.log2(f0 / 440)))
        y = sim.play(inst, note, **P)[int(1.4 * 48000):int(3.9 * 48000)]
        pr = F.partial_levels(rec, f0)
        pm = F.partial_levels(y, f0)
        c = min(len(pr), len(pm))
        # (and each note's loudness against the recording's: rms in dB)
        loud = 20 * np.log10(np.sqrt(np.mean(rec ** 2)) / np.sqrt(np.mean(y ** 2)))
        diffs.append((pr[:c, 0], pr[:c, 1] - pm[:c, 1], loud, 10 ** (pm[:c, 1] / 10)))
    mean_loud = np.mean([dl for _, _, dl, _ in diffs])
    level_sum = np.zeros(len(gains))
    level_weight = np.zeros(len(gains))
    for freqs, d, loud, power in diffs:
        shape_err = d - d.mean()
        level_err = loud - mean_loud
        errs.append(np.mean(np.abs(shape_err)) + LEVEL * abs(level_err))
        # (a note's loudness is its strongest partials': the level error
        # goes to the resonators under them, by power)
        weight = power / power.sum()
        for f, dv, w in zip(freqs, shape_err, weight):
            j = int(np.argmin(np.abs(np.log(centres / f))))
            corr[j].append(dv)
            level_sum[j] += w * level_err
            level_weight[j] += w
    e = np.mean(errs)
    print(inst, step, round(e, 2), flush=True)
    if e < best[0]:
        best = (e, gains.copy())
    level_fix = np.where(level_weight > 0.05, level_sum / np.maximum(level_weight, 1e-9), 0.0)
    gains = gains + 0.5 * (np.array([np.mean(c) if c else 0 for c in corr]) + LEVEL * level_fix)
    gains -= gains.max()
    gains = np.maximum(gains, -40) + offset
json.dump(dict(err=best[0], gains=best[1].tolist()), open(f"body2-{inst}.json", "w"))
