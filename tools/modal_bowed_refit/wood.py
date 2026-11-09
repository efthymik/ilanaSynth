"""The wood: what sits between and under the partials, and how each partial's
level flutters by where it sits. wood.py ['{"param": value}'] (run from the
working folder holding arco/<inst>/): Spitfire against the Python twin over
the look_score.py notes.

Measures (sustain, 1.0 to 3.8 s):
  floor   the level between the partials per octave (their vibrato skirts
          masked out), against the whole held power, dB; "under" is all of it
          below the fundamental. A clean, woody note has its noise where the
          body rings, not stuck to the partials.
  amoct   each partial's fast flutter (level std, dB, 10 to 40 Hz) by the
          octave it sits in: Spitfire's low partials hold steady and its upper
          ones shake.
"""
import json
import sys
from multiprocessing import Pool

import numpy as np
from scipy.signal import hilbert

import look_score
import sim
import wobble

SR = 48000
BANDS = (63, 125, 250, 500, 1000, 2000, 4000, 8000)
OCT = tuple(62.5 * 2 ** k for k in range(8))


def floor(x, f0, a=1.0, b=3.8):
    s = x[int(a * SR):int(b * SR)]
    S = np.abs(np.fft.rfft(s * np.hanning(len(s)))) ** 2
    f = np.fft.rfftfreq(len(s), 1 / SR)
    tot = S[(f > 40) & (f < 16000)].sum()
    mask = np.ones_like(f, bool)
    for fh in np.arange(1, int(16000 / f0) + 2) * f0:
        mask &= np.abs(f - fh) > 0.02 * fh + 8
    out = {}
    under = (f > 40) & (f < 0.98 * f0 - 8)
    out["under"] = float(10 * np.log10(S[under].sum() / tot + 1e-20)) if under.any() else np.nan
    for lo in BANDS:
        band = (f >= lo) & (f < 2 * lo)
        m = band & mask & (f > f0)
        # the floor's density, summed over the octave, against the whole
        out[lo] = float(10 * np.log10(S[m].mean() * lo / f[1] / tot + 1e-20)) if m.sum() > 0.05 * band.sum() else np.nan
    return out


def partial_flutter(x, f0, top=8000):
    seg = x[int(0.9 * SR):int(3.9 * SR)]
    keep = slice(int(0.1 * SR), int(2.9 * SR))
    dec = 48
    rows = []
    for h in range(1, 64):
        fh = h * f0
        if fh > top:
            break
        w = min(0.45 * f0, 0.45 * fh)
        y = wobble.bp(seg, max(20, fh - w), min(SR / 2 * 0.98, fh + w))
        db = 20 * np.log10(np.abs(hilbert(y))[keep][::dec] + 1e-9)
        rows.append((fh, db.mean(), wobble.envelope_rate(db, SR / dec, 10, 40).std()))
    R = np.array(rows)
    return R[R[:, 1] > R[:, 1].max() - 30]


def measure(x, f0, rows=None):
    """rows: wobble.measure(..., rows=True)'s partial rows, if already made."""
    if rows is None:
        R = partial_flutter(x, f0)
    else:
        R = rows[rows[:, 1] > rows[:, 1].max() - 30]
    return dict(floor=floor(x, f0), flutter=R)


def summary(M):
    """Medians over notes: the floor per band, and the flutter per octave."""
    fl = {k: float(np.nanmedian([m["floor"][k] for m in M])) if not all(np.isnan(m["floor"][k]) for m in M) else np.nan
          for k in ["under", *BANDS]}
    am = {}
    for lo in OCT:
        v = [r[2] for m in M for r in m["flutter"] if lo <= r[0] < 2 * lo]
        am[lo] = float(np.median(v)) if len(v) >= 3 else np.nan
    return fl, am


def term(a, b):
    """How far the twin's wood is from Spitfire's: floor dB (x0.5) and the
    flutter by octave (x2)."""
    (fa, aa), (fb, ab) = a, b
    d1 = [abs(fa[k] - fb[k]) for k in fa if np.isfinite(fa[k]) and np.isfinite(fb[k])]
    d2 = [abs(aa[k] - ab[k]) for k in aa if np.isfinite(aa[k]) and np.isfinite(ab[k])]
    return 0.5 * float(np.mean(d1)) + 2 * float(np.mean(d2))


def one(args):
    inst, n, kw = args
    f0 = 440 * 2 ** ((n - 69) / 12)
    return inst, measure(look_score.ref(inst, n), f0), measure(sim.play(inst, n, **kw), f0)


if __name__ == "__main__":
    kw = json.loads(sys.argv[1]) if len(sys.argv) > 1 else {}
    with Pool(4) as p:
        R = p.map(one, [(i, n, kw) for i, n in look_score.SET])
    for inst in ("violin", "cello"):
        a = summary([r[1] for r in R if r[0] == inst])
        b = summary([r[2] for r in R if r[0] == inst])
        print(f"{inst}: term {term(a, b):.2f}")
        print("  floor   " + "".join(f"{str(k):>7s}" for k in a[0]))
        print("  Spitfire" + "".join(f"{v:7.1f}" for v in a[0].values()))
        print("  twin    " + "".join(f"{v:7.1f}" for v in b[0].values()))
        print("  flutter " + "".join(f"{k:7.0f}" for k in a[1]))
        print("  Spitfire" + "".join(f"{v:7.2f}" for v in a[1].values()))
        print("  twin    " + "".join(f"{v:7.2f}" for v in b[1].values()))
