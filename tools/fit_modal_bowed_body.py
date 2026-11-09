#!/usr/bin/env python3
"""Fit the Modal Bowed exciter's bodies (src/dsp/ModalBowedBodies.h) to
recordings of a violin, a viola and a cello.

The body is a parallel bank of fixed-Hz resonators on one grid: the six
named low modes of a violin (Holm & Valimaki, EUSIPCO 2000: C1 185, A0 265,
C2 395, A1 430, C3 550, C4 665 Hz) and one resonator per 1/8 octave from
700 Hz to 9 kHz (bandwidth f / 16). A viola's and a cello's bodies are that
grid divided by their size against a violin (1.17 and 2.1), with gains of
their own. Each instrument's gains are fitted the same way: the modal bowed
string (the engine's, ModalBowed.h, at the card's defaults) plays every
recorded note, and each resonator's gain moves by the average dB its
partials miss the recording's by, over eight passes. The three bodies are
then levelled so a note in the middle of each instrument comes out equally
loud.

    tools/fit_modal_bowed_body.py --violin DIR --viola DIR --cello DIR [--out src/dsp/ModalBowedBodies.h]

Each DIR holds the University of Iowa 2012 arco mf files of that instrument
(theremin.music.uiowa.edu, MISviolin2012 / MISviola2012 / MIScello2012),
or one WAV per note named like Iowa's (x.D4D4.stereo.wav), with half a
second of silence before the note. The bodies in the repo are fitted to
ilana's renders of render-midi/<violin|viola|cello>-arco-full-range.mid
(Spitfire String Quartet, Long Normale, dynamics 90, reverb off; the
velocity 80 pass): --source names them in the header. Needs numpy, scipy and soundfile; numba speeds the
string up about fifty times.
"""

import argparse
import glob
import os

import numpy as np
import soundfile as sf
from scipy.signal import butter, lfilter, resample_poly, sosfiltfilt

try:
    from numba import njit
except ImportError:  # (slow but the same)
    def njit(f):
        return f

SR = 48000
NAMED = [(185, 20), (265, 15), (395, 25), (430, 20), (550, 30), (665, 30)]
GRID = NAMED + [(float(f), float(f) / 16) for f in np.geomspace(700, 9000, 30)]
SIZES = {"violin": 1.0, "viola": 1.17, "cello": 2.1}
MAX_MODES = 64
MIDDLE = {"violin": 293.66, "viola": 196.0, "cello": 98.0}  # D4, G3, G2


@njit
def bowed_string(f0, seconds, sr, modes, beta, loss0, loss1, b, pressure, vbow, seed):
    """The engine's bowed string (ModalBowed::process at the card's defaults,
    velocity 0.5), returning the bridge force."""
    k = 1.0 / sr
    n = np.arange(1, modes + 1).astype(np.float64)
    hz = np.minimum(n * f0 * np.sqrt(1.0 + b * n * n), sr * 0.49)
    sigma = loss0 + loss1 * hz * hz
    decay = np.exp(-sigma * k)
    turn = 2.0 * decay * np.cos(2 * np.pi * hz * k)
    fall = decay * decay
    shape = np.sin(n * np.pi * beta)
    push = 2.0 * k * shape / max(1e-6, np.sum(shape * shape))
    out = n / modes
    q = np.zeros(modes)
    qp = np.zeros(modes)
    total = int(seconds * sr)
    y = np.zeros(total)
    attack = 0.05 * min(2.5, max(1.0, 220.0 / f0))
    ac = 1.0 - np.exp(-1.0 / (attack * sr))
    bite_fall = np.exp(-1.0 / (0.08 * sr))
    env = 0.0
    bite = 1.0
    s = seed
    for i in range(total):
        nxt = turn * q - fall * qp
        v = np.sum(shape * (nxt - qp)) * 0.5 * sr
        env += (1.0 - env) * ac
        s = (s * 1664525 + 1013904223) % 4294967296
        jitter = ((s >> 8) * (2.0 / 16777216.0) - 1.0) * 0.0035
        bow = vbow * env * (1.0 + 0.6 * 0.5 * bite) * (1.0 + jitter)
        bite *= bite_fall
        d = bow - v
        slope = 5.0 - 4.0 * pressure
        table = min(1.0, (abs(d * slope) + 0.75) ** -4.0)
        force = d * table * env
        nxt = nxt + push * force
        qp = q
        q = nxt
        y[i] = np.sum(out * q)
    return y


def string_for(f0, seconds=1.8, seed=1):
    modes = int(max(1, min(MAX_MODES, min(9000.0, SR * 0.45) / f0)))
    return bowed_string(f0, seconds, SR, modes, 0.11, 0.8, 3.0e-5, 1.0e-5, 0.5, 0.05 + 0.3 * 0.5, seed)


def resonator(f, bw):
    r = np.exp(-np.pi * bw / SR)
    return [(1 - r * r) / 2, 0, -(1 - r * r) / 2], [1, -2 * r * np.cos(2 * np.pi * f / SR), r * r]


def body(x, size, gains_db):
    y = np.zeros_like(x)
    for (f, bw), g in zip(GRID, gains_db):
        if f / size >= SR * 0.45:
            continue
        b, a = resonator(f / size, bw / size)
        y += 10 ** (g / 20) * lfilter(b, a, x)
    return y


def ltas(x, n=16384):
    fr = np.fft.rfftfreq(n, 1 / SR)
    acc = np.zeros(len(fr))
    count = 0
    for i in range(0, len(x) - n, n // 2):
        acc += np.abs(np.fft.rfft(x[i:i + n] * np.hanning(n))) ** 2
        count += 1
    return fr, 10 * np.log10(acc / max(count, 1) + 1e-20)


def partial_levels(x, f0, top=9000.0):
    fr, level = ltas(x)
    return np.array([(m * f0, level[(fr > m * f0 * 0.97) & (fr < m * f0 * 1.03)].max())
                     for m in range(1, int(top / f0)) if m * f0 < top])


NAMES = {"C": 0, "Db": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "Gb": 6, "G": 7, "Ab": 8, "A": 9, "Bb": 10, "B": 11}


def first_note(path):
    """The MIDI note an Iowa file starts on (its name: ...sulG.G3B3...), or None."""
    import re
    m = re.search(r"\.([A-G]b?)(\d)([A-G]b?)(\d)\.(mono|stereo)", os.path.basename(path))
    return None if not m else 12 * (int(m.group(2)) + 1) + NAMES[m.group(1)]


def pitch(x, expected=None):
    seg = x[:int(0.5 * SR)]
    n = 1 << 18
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    fr = np.fft.rfftfreq(n, 1 / SR)
    cands = (np.arange(55, 2000, 0.25) if expected is None
             else np.arange(expected * 2 ** (-1 / 12), expected * 2 ** (0.6 / 12), 0.05))
    score = [np.sum(np.interp(f0 * np.arange(1, 6), fr, sp)) for f0 in cands]
    f0 = cands[int(np.argmax(score))]
    k = (fr > f0 * 0.97) & (fr < f0 * 1.03)
    return float(fr[k][np.argmax(sp[k])])


def notes_in(folder):
    """Steady stretches of every note: 0.5 to 1.7 s after each onset."""
    hp = butter(4, 50, "highpass", fs=SR, output="sos")
    found = []
    for path in sorted(glob.glob(os.path.join(folder, "*.aif")) + glob.glob(os.path.join(folder, "*.wav"))):
        x, rate = sf.read(path, always_2d=True)
        x = x.mean(axis=1)
        if rate != SR:
            from math import gcd
            g = gcd(SR, rate)
            x = resample_poly(x, SR // g, rate // g)
        x = sosfiltfilt(hp, x)
        hop = int(0.01 * SR)
        n = len(x) // hop
        db = 20 * np.log10(np.sqrt((x[:n * hop].reshape(n, hop) ** 2).mean(axis=1)) + 1e-9)
        last = -100
        start = first_note(path)
        index = 0
        for i in range(30, n):
            if db[i] > db.max() - 30 and db[i] - db[i - 30:i - 3].min() > 18 and i - last > 150:
                last = i
                # Iowa's files are chromatic runs from the note in their name.
                expected = None if start is None else 440.0 * 2 ** ((start + index - 69) / 12)
                index += 1
                seg = x[i * hop + int(0.5 * SR):i * hop + int(1.7 * SR)]
                if len(seg) == int(1.2 * SR) and np.sqrt(np.mean(seg ** 2)) > 10 ** (-50 / 20):
                    f0 = pitch(seg, expected)
                    if f0 < 1100:  # (above, too few partials to place a body)
                        found.append((seg, f0))
    return found


def fit(name, folder, max_notes=24):
    size = SIZES[name]
    notes = notes_in(folder)
    notes = notes[::max(1, len(notes) // max_notes)][:max_notes]
    print("%s: %d notes, %.0f to %.0f Hz" % (name, len(notes), min(f for _, f in notes), max(f for _, f in notes)), flush=True)
    sources = [string_for(f0, seed=i + 1)[int(0.6 * SR):] for i, (_, f0) in enumerate(notes)]
    centres = np.array([f / size for f, _ in GRID])
    gains = np.zeros(len(GRID))
    best, best_error = gains.copy(), 1e9
    for step in range(10):
        corrections = [[] for _ in GRID]
        errors = []
        for (rec, f0), src in zip(notes, sources):
            pr = partial_levels(rec, f0)
            pm = partial_levels(body(src, size, gains), f0)
            count = min(len(pr), len(pm))
            d = pr[:count, 1] - pm[:count, 1]
            d -= d.mean()
            errors.append(np.mean(np.abs(d)))
            for f, dv in zip(pr[:count, 0], d):
                corrections[int(np.argmin(np.abs(np.log(centres / f))))].append(dv)
        print("  pass %d: partial error %.2f dB" % (step, np.mean(errors)), flush=True)
        if np.mean(errors) < best_error:
            best, best_error = gains.copy(), np.mean(errors)
        gains += 0.5 * np.array([np.mean(c) if c else 0.0 for c in corrections])
        gains -= gains.max()
        # (Within 40 dB of the loudest: a resonator few partials reach is
        # otherwise pushed towards nothing by a single loud miss.)
        gains = np.maximum(gains, -40.0)
    return best, best_error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--violin", required=True)
    parser.add_argument("--viola", required=True)
    parser.add_argument("--cello", required=True)
    parser.add_argument("--out", default="src/dsp/ModalBowedBodies.h")
    parser.add_argument("--source", default="the University of Iowa 2012 violin, viola and cello (arco, mf)",
                        help="what the recordings are, for the generated header's comment")
    args = parser.parse_args()

    fitted, errors = {}, {}
    for name in ("violin", "viola", "cello"):
        fitted[name], errors[name] = fit(name, getattr(args, name))

    # Level: a middle note of each through its body, as loud as the violin's.
    level = {}
    for name in fitted:
        y = body(string_for(MIDDLE[name], 1.5)[int(0.5 * SR):], SIZES[name], fitted[name])
        level[name] = 20 * np.log10(np.sqrt(np.mean(y ** 2)) + 1e-20)
    for name in fitted:
        fitted[name] = fitted[name] - (level[name] - level["violin"])

    def row(values, fmt):
        return ", ".join(fmt % v for v in values)

    text = """#pragma once

// The Modal Bowed exciter's bodies (ModalBowed.h), fitted by
// tools/fit_modal_bowed_body.py to %s:
// one grid of resonators (Hz and bandwidth for a violin; a viola's and a
// cello's are divided by their size), and each instrument's gains in dB.
// Partial error after the fit: violin %.1f dB, viola %.1f dB, cello %.1f dB.
// Generated: edit the tool, not this file.
struct ModalBowedBodies
{
    static constexpr int count = %d;
    static constexpr double viola = %.2f, cello = %.2f; // sizes against a violin
    static constexpr float hz[count] { %s };
    static constexpr float bandwidth[count] { %s };
    static constexpr float violin[count] { %s };
    static constexpr float violas[count] { %s };
    static constexpr float cellos[count] { %s };
};
""" % (args.source, errors["violin"], errors["viola"], errors["cello"], len(GRID), SIZES["viola"], SIZES["cello"],
       row([f for f, _ in GRID], "%.2ff"), row([b for _, b in GRID], "%.2ff"),
       row(fitted["violin"], "%.2ff"), row(fitted["viola"], "%.2ff"), row(fitted["cello"], "%.2ff"))
    with open(args.out, "w") as f:
        f.write(text)
    print("wrote", args.out)


if __name__ == "__main__":
    main()
