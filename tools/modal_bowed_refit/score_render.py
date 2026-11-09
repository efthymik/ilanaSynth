"""Score plugin renders on the picture score: score_render.py PREFIX...

Renders PREFIX-violin.wav (demo-violin.mid: 62 69 76 81) and PREFIX-cello.wav
(demo-cello.mid: 36 43 48 53), 4 s notes every 5 s, e.g.
  ILANA_SET="osc1_excite=Modal Bowed,amp_release=0.8" ilanaMidiRender demo-violin.mid P-violin.wav init-modal
  ILANA_SET="osc1_excite=Modal Bowed,osc1_modal_size=2.1,amp_release=0.8" ilanaMidiRender demo-cello.mid P-cello.wav init-modal
"""
import os
import sys

import numpy as np
import soundfile as sf

import look
from look_score import ref

DEMO = {"violin": (62, 69, 76, 81), "cello": (36, 43, 48, 53)}
for prefix in sys.argv[1:]:
    for inst, notes in DEMO.items():
        path = f"{prefix}-{inst}.wav"
        if not os.path.exists(path):
            continue
        y, _ = sf.read(path)
        y = y.mean(1) if y.ndim > 1 else y
        R = [look.dist(ref(inst, n), y[int(i * 5 * 48000):int((i * 5 + 4.8) * 48000)], 440 * 2 ** ((n - 69) / 12)) for i, n in enumerate(notes)]
        print(prefix, inst, {q: round(float(np.mean([r[q] for r in R])), 2) for q in ("sustain", "wobble", "attack", "release", "plevel", "vibam", "slow", "total")})
