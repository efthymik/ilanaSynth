"""The guitar's picture score: how far a plucked note looks from a reference
render (refs.py cuts it; REFS=dir picks the reference set, default refs).
Terms (dB, mean absolute difference, levels floored 60 dB under the note's
loudest moment; lower is closer):
  onset   1/6-octave bands in 5 ms frames over the first 60 ms (the pluck)
  decay   1/6-octave bands at 0.1 to 2.2 s (log-spaced frames, 43 ms)
  partial each harmonic's level at 0.05 to 1.5 s (up to 10 kHz)
  noise   the floor between the partials, per octave, at 0.2 to 1.5 s
Both sounds are first levelled to the same energy over 0 to 0.5 s.
    score.py TAG [notes...]      (out/TAG/ against refs/)
"""
import sys

import numpy as np
import soundfile as sf
from scipy.signal import stft

SR = 48000
EDGES = np.geomspace(80, 16000, 47)
FLOOR = 60.0
REFS = __import__("os").environ.get("REFS", "refs")


def bands(x, nper, hop):
    f, t, Z = stft(x, SR, nperseg=nper, noverlap=nper - hop, boundary=None, padded=False)
    P = np.abs(Z) ** 2
    B = np.array([P[(f >= a) & (f < b)].sum(0) for a, b in zip(EDGES, EDGES[1:]) if ((f >= a) & (f < b)).any()])
    return t, 10 * np.log10(B + 1e-14)


def floor(B):
    return np.maximum(B, B.max() - FLOOR)


def parts(x, f0):
    x = x / np.sqrt(np.mean(x[: SR // 2] ** 2) + 1e-20)
    out = {}
    t, B = bands(x, 256, 240)  # 5 ms frames
    out["onset"] = floor(B[:, t < 0.06])
    t, B = bands(x, 2048, 1024)
    pick = [int(np.argmin(np.abs(t - q))) for q in np.geomspace(0.1, 2.2, 12)]
    out["decay"] = floor(B[:, pick])
    rows, noise = [], []
    for q in (0.05, 0.1, 0.2, 0.4, 0.8, 1.5):
        a = int(q * SR)
        seg = x[a:a + 8192] * np.hanning(8192)
        sp = np.abs(np.fft.rfft(seg)) ** 2
        fr = np.fft.rfftfreq(8192, 1 / SR)
        ks = [k for k in range(1, 60) if k * f0 < 10000]
        rows.append([10 * np.log10(sp[np.abs(fr - k * f0) < 0.04 * k * f0 + 6].max() + 1e-14) for k in ks])
        mask = np.zeros_like(fr, bool)
        for k in range(1, 200):
            mask |= np.abs(fr - k * f0) < 0.06 * f0 + 6
        noise.append([10 * np.log10(np.median(sp[(~mask) & (fr >= lo) & (fr < 2 * lo)]) + 1e-14) if lo >= 125 else 0
                      for lo in (125, 250, 500, 1000, 2000, 4000, 8000)])
    out["partial"] = floor(np.array(rows))
    out["noise"] = np.array(noise)
    return out


def dist(ref, syn, f0):
    a, b = parts(ref, f0), parts(syn, f0)
    d = {}
    for k in a:
        n = min(a[k].shape[-1], b[k].shape[-1])
        d[k] = float(np.mean(np.abs(a[k][..., :n] - b[k][..., :n])))
    d["total"] = d["onset"] + d["decay"] + d["partial"] + 0.5 * d["noise"]
    return d


def note_f0(note):
    return 440 * 2 ** ((note - 69) / 12)


def run(tag, keys):
    R = {}
    for k in keys:
        n = int(k.split("-")[0])
        ref, _ = sf.read("%s/%s.wav" % (REFS, k))
        syn, _ = sf.read("out/%s/%s.wav" % (tag, k))
        m = min(len(ref), len(syn))
        R[k] = dist(ref[:m], syn[:m], note_f0(n))
    return R


if __name__ == "__main__":
    import glob
    import os
    tag = sys.argv[1]
    keys = sys.argv[2:] or sorted(os.path.basename(p)[:-4] for p in glob.glob(REFS + "/*.wav"))
    R = run(tag, keys)
    for k, d in R.items():
        print(k, {q: round(v, 1) for q, v in d.items()})
    print("MEAN", {q: round(float(np.mean([d[q] for d in R.values()])), 2) for q in next(iter(R.values()))})
