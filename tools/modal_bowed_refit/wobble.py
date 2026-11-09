"""The bow's fine randomness, measured: wobble.py ['{"param": value}'] (run from the
working folder holding arco/<inst>/). Spitfire against the Python twin over the
look_score.py notes, partial by partial (each harmonic band-passed, its level and
frequency from the analytic signal, so modulation up to ~40 Hz shows; a
spectrogram's window smooths it away).

Measures (sustain, 1.0 to 3.8 s):
  pitch   cents of the low partials: vib (3-9 Hz std), drift (<2 Hz), jitter (10-40 Hz)
  cycles  the vibrato cycle to cycle: rate and depth spread (std / mean)
  am      each partial's level (dB std) in 0.3-2, 3-9, 10-20, 20-40 Hz, weighted
          to the partials within 30 dB of the loudest
  corr    how together the partials move (mean pairwise correlation) slow / fast
  bright  the spectral centroid's wobble (% std), slow and at the vibrato rate
  noise   the noise between the partials: level under them (dB) and how much it
          flutters (dB std, 2-40 Hz)
  skirt   each partial's skirt: power (dB per Hz against the partial) 10-20,
          20-40 and 40-80 Hz off it once the vibrato's slow bend is taken out
"""
import json
import sys
from multiprocessing import Pool

import numpy as np
from scipy.signal import butter, hilbert, sosfiltfilt

import look_score
import sim

SR = 48000
BANDS = {"slow": (0.3, 2), "vib": (3, 9), "flut": (10, 20), "fast": (20, 40)}
SKIRT = ((10, 20), (20, 40), (40, 80))


def bp(x, lo, hi, fs=SR, order=4):
    return sosfiltfilt(butter(order, [lo, hi], "bandpass", fs=fs, output="sos"), x)


def envelope_rate(e, fs, lo, hi):
    if hi >= fs / 2:
        hi = fs / 2 * 0.95
    return bp(e, lo, hi, fs=fs, order=2)


def measure(x, f0, top=8000):
    a, b = int(0.9 * SR), int(3.9 * SR)
    seg = x[a:b]
    keep = slice(int(0.1 * SR), int(2.9 * SR))  # 1.0 to 3.8 s, edges of the filters left out
    dec = 48  # envelopes at 1 kHz
    fs = SR / dec
    levels, cents, skirts = [], [], []
    for h in range(1, 60):
        fh = h * f0
        if fh > top:
            break
        w = min(0.45 * f0, 0.45 * fh)
        y = bp(seg, max(20, fh - w), min(SR / 2 * 0.98, fh + w))
        z = hilbert(y)
        env = np.abs(z)[keep][::dec]
        db = 20 * np.log10(env + 1e-9)
        levels.append(db)
        # The partial's skirt: its complex envelope with the slow phase (the
        # vibrato, the drift) taken out; the power at each offset from the
        # partial, against the partial (AM and FM together, what the eye
        # sees as a grainy or a clean line).
        zz = z[keep][::dec]
        ph = np.unwrap(np.angle(zz))
        slowph = sosfiltfilt(butter(2, 8, "lowpass", fs=fs, output="sos"), ph)
        r = zz * np.exp(-1j * slowph) / np.abs(zz).mean()
        r = r - r.mean()
        S = np.abs(np.fft.fft(r * np.hanning(len(r)))) ** 2 / np.sum(np.hanning(len(r)) ** 2) / fs
        fr = np.abs(np.fft.fftfreq(len(r), 1 / fs))
        skirts.append([10 * np.log10(S[(fr >= lo) & (fr < hi)].mean() + 1e-20) for lo, hi in SKIRT])
        if h <= 3:
            ph = np.unwrap(np.angle(z))
            inst = np.diff(ph) * SR / (2 * np.pi)
            inst = inst[keep][::dec]
            cents.append((h, 1200 * np.log2(np.maximum(inst, 1) / fh), db.mean()))
    L = np.array(levels)
    mean = L.mean(1)
    w = (mean > mean.max() - 30).astype(float)
    w /= w.sum()
    out = {}
    for k, (lo, hi) in BANDS.items():
        out["am_" + k] = float(np.sum(w * np.array([envelope_rate(l, fs, lo, hi).std() for l in L])))
    K = np.array(skirts)
    for j, (lo, hi) in enumerate(SKIRT):
        out["skirt_%d" % lo] = float(np.sum(w * K[:, j]))
    # how together the partials move
    for k in ("slow", "fast"):
        lo, hi = BANDS[k]
        F = np.array([envelope_rate(l, fs, lo, hi) for l, ww in zip(L, w) if ww > 0])
        if len(F) > 2:
            C = np.corrcoef(F)
            out["corr_" + k] = float(C[np.triu_indices(len(F), 1)].mean())
    # pitch from the loudest of the first three partials
    h, c, _ = max(cents, key=lambda v: v[2])
    c = np.clip(c - np.median(c), -200, 200)
    out["pitch_vib"] = float(envelope_rate(c, fs, 3, 9).std())
    out["pitch_drift"] = float(sosfiltfilt(butter(2, 2, "lowpass", fs=fs, output="sos"), c).std())
    out["pitch_jit"] = float(envelope_rate(c, fs, 10, 40).std())
    # vibrato cycle to cycle
    v = envelope_rate(c, fs, 3, 9)
    up = np.where((v[:-1] < 0) & (v[1:] >= 0))[0]
    if len(up) > 4:
        per = np.diff(up) / fs
        amp = np.array([v[i:j].max() - v[i:j].min() for i, j in zip(up[:-1], up[1:])])
        out["cyc_rate"] = float(per.std() / per.mean())
        out["cyc_depth"] = float(amp.std() / amp.mean())
    # brightness: centroid in harmonic numbers over time
    P = 10 ** (L / 10)
    hn = np.arange(1, len(L) + 1)[:, None]
    cen = (P * hn).sum(0) / P.sum(0)
    rel = cen / cen.mean()
    out["bright_slow"] = float(100 * sosfiltfilt(butter(2, 2, "lowpass", fs=fs, output="sos"), rel - 1).std())
    out["bright_vib"] = float(100 * envelope_rate(rel, fs, 3, 9).std())
    # the noise between the partials (halfway between harmonics 2..12)
    noise, nfl = [], []
    for h in range(2, 13):
        fh = (h + 0.5) * f0
        if fh > top:
            break
        w2 = 0.15 * f0
        y = bp(seg, fh - w2, fh + w2)
        env = np.abs(hilbert(y))[keep][::dec]
        db = 20 * np.log10(env + 1e-9)
        noise.append(db.mean() - 0.5 * (L[h - 1].mean() + L[h].mean()) if h < len(L) else db.mean() - L[h - 1].mean())
        nfl.append(envelope_rate(db, fs, 2, 40).std())
    out["noise_under"] = float(np.mean(noise))
    out["noise_flutter"] = float(np.mean(nfl))
    return out


def one(args):
    inst, n, kw = args
    f0 = 440 * 2 ** ((n - 69) / 12)
    r = measure(look_score.ref(inst, n), f0)
    s = measure(sim.play(inst, n, **kw), f0)
    return inst, n, r, s


if __name__ == "__main__":
    kw = json.loads(sys.argv[1]) if len(sys.argv) > 1 else {}
    with Pool(4) as p:
        R = p.map(one, [(i, n, kw) for i, n in look_score.SET])
    keys = list(R[0][2].keys())
    for inst in ("violin", "cello"):
        rr = [r for r in R if r[0] == inst]
        print(f"{inst:7s} {'measure':14s} {'Spitfire':>9s} {'twin':>9s}")
        for k in keys:
            a = np.median([r[2].get(k, np.nan) for r in rr])
            b = np.median([r[3].get(k, np.nan) for r in rr])
            print(f"        {k:14s} {a:9.3f} {b:9.3f}")
    json.dump([[i, n, r, s] for i, n, r, s in R], open("wobble.json", "w"))
