"""Spectrograms of Spitfire against the Python twin: spec_sim.py out.png ['{"param": value}']."""
import json
import sys

import matplotlib
import numpy as np
import soundfile as sf

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from scipy.signal import spectrogram  # noqa: E402

import look  # noqa: E402
import sim  # noqa: E402

NAMES = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
out = sys.argv[1]
kw = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
kw = {k: v for k, v in kw.items() if k in sim.DEFAULT}
rows = [("violin", 62), ("violin", 76), ("cello", 43), ("cello", 50)]
fig, ax = plt.subplots(len(rows), 2, figsize=(14, 3.2 * len(rows)))
for r, (inst, n) in enumerate(rows):
    name = NAMES[n % 12] + str(n // 12 - 1)
    x, sr = sf.read(f"arco/{inst}/r.{name}{name}.stereo.wav")
    x = x.mean(1)[24000:24000 + int(4.8 * sr)]
    e = sim.play(inst, n, **kw)
    print(inst, name, {k: round(float(v), 1) for k, v in look.dist(x, e, 440 * 2 ** ((n - 69) / 12)).items()})
    for c, (label, y) in enumerate((("Spitfire", x), ("engine", e))):
        f, t, S = spectrogram(y, sr, nperseg=2048, noverlap=1792)
        S = 10 * np.log10(S + 1e-14)
        S -= S.max()
        ax[r, c].pcolormesh(t, f, S, vmin=-90, vmax=0, shading="auto", cmap="magma")
        ax[r, c].set_yscale("symlog", linthresh=500)
        ax[r, c].set_ylim(50, 16000)
        ax[r, c].set_title(f"{inst} {name} {label}")
plt.tight_layout()
plt.savefig(out, dpi=70)
