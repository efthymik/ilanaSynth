#!/usr/bin/env python3
"""Build the Modal Guitar table (content/guitar/modal-guitar.bin): the Modal
Piano's format (tools/build_modal_piano.py, src/dsp/ModalPianoTable.h), from
recordings of single guitar notes.

Two kinds of source:

  --iowa DIR    the University of Iowa nylon-string guitar (Musical
                Instrument Samples, theremin.music.uiowa.edu/MISguitar.html):
                every file Guitar.<pp|mf|ff>.<string>.<range>.stereo.aif in
                DIR, several notes per file. Each note is taken from the
                string where it sits lowest on the neck.
  --render WAV  a render of a guitar plugin from
                ilanasynth/modal-guitar-violin-research/render-midi/guitar-full-range.mid
                (E2..E6, velocities 40/80/120, one note per --slot seconds).

Recorded notes are often cut short (Iowa's by the next note after 1 to 3 s):
each level curve past the end of its note carries on along the slope of its
last measured half second, so the table's time grid reaches 5.5 s for every
note. The partials' frequencies are kept as ratios to the note's measured
pitch (Iowa's guitar sits about 40 cents flat), so the engine plays in tune.

    tools/build_modal_guitar.py --iowa ~/iowa-guitar [--out content/guitar/modal-guitar.bin]

Needs numpy and soundfile (libsndfile reads the AIFF files).
"""

import argparse
import glob
import os
import re
import struct
import sys

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_modal_piano as mp  # noqa: E402

TIMES = [0.05, 0.08, 0.12, 0.2, 0.3, 0.45, 0.7, 1.0, 1.5, 2.0, 3.0, 4.0, 5.5]
OPEN = {"sulE": 40, "sulA": 45, "sulD": 50, "sulG": 55, "sulB": 59, "sul_E": 64}
DYNAMICS = {"pp": 35, "mf": 80, "ff": 120}


def highpass(x, rate, hz=60.0):
    """A 4th-order high-pass (the Iowa room's rumble), zero phase."""
    from scipy.signal import butter, sosfiltfilt
    sos = butter(4, hz, "highpass", fs=rate, output="sos")
    return sosfiltfilt(sos, x, axis=0)


def onsets(x, rate):
    """Note starts: the level jumps 18 dB over the last 300 ms."""
    hop = int(0.01 * rate)
    n = len(x) // hop
    m = x[:n * hop].mean(axis=1) if x.ndim > 1 else x[:n * hop]
    db = 20 * np.log10(np.sqrt((m.reshape(n, hop) ** 2).mean(axis=1)) + 1e-9)
    top = db.max()
    found, last = [], -100
    for i in range(30, n):
        if db[i] > top - 30 and db[i] - db[i - 30:i - 3].min() > 18 and i - last > 80:
            found.append(i * hop)
            last = i
    return found


def pitch(x, rate):
    seg = x[int(0.15 * rate):int(0.65 * rate)]
    seg = seg.mean(axis=1) if seg.ndim > 1 else seg
    n = 1 << 17
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    fr = np.fft.rfftfreq(n, 1 / rate)
    cands = np.arange(70, 1400, 0.25)
    score = [np.sum(np.interp(f0 * np.arange(1, 6), fr, sp)) for f0 in cands]
    f0 = cands[int(np.argmax(score))]
    k = (fr > f0 * 0.97) & (fr < f0 * 1.03)
    return float(fr[k][np.argmax(sp[k])])


def carry_on(levels, valid):
    """Each row's levels past the last valid time: along the slope of its
    last half second of measurements (at least 1 dB/s down)."""
    out = levels.copy()
    times = np.array(TIMES)
    v = np.array(valid)
    if v.all() or v.sum() < 2:
        return out
    last = int(np.where(v)[0][-1])
    end = times[last]
    use = np.where(v & (times >= max(0.2, end - 0.5)))[0]
    if len(use) < 2:
        use = np.where(v)[0][-2:]
    for row in out:
        slope = np.polyfit(times[use], row[use], 1)[0] if len(use) >= 2 else -10.0
        slope = min(slope, -1.0)
        for t in range(last + 1, len(TIMES)):
            row[t] = max(-160.0, row[last] + slope * (times[t] - end))
    return out


def analyse(x, rate, f0):
    size = 16384 if f0 < 60 else 8192
    freqs = np.fft.rfftfreq(size, 1 / rate)
    partials = mp.track(mp.spectrum(x, int(0.25 * rate), size), freqs, f0)
    half = max(2.0, 0.2 * f0)
    levels = np.full((len(partials), len(TIMES)), -160.0)
    valid = []
    for t, when in enumerate(TIMES):
        start = max(0, int(when * rate) - size // 2)
        valid.append(start + size <= len(x))
        mag = mp.spectrum(x, start, size)
        for n, f in enumerate(partials):
            k = (freqs > f - half) & (freqs < f + half)
            peak = mag[k].max() if k.any() else mag[int(np.argmin(np.abs(freqs - f)))]
            levels[n, t] = 20 * np.log10(peak + 1e-9)
    return partials / f0, carry_on(levels, valid), valid


def iowa_notes(folder):
    """{(midi, dynamic): (fret, segment, rate)}: each note from the string where its fret is lowest."""
    found = {}
    for path in sorted(glob.glob(os.path.join(folder, "Guitar.*.stereo.aif"))):
        m = re.match(r"Guitar\.(pp|mf|ff)\.(sul_?E|sulA|sulD|sulG|sulB)\.", os.path.basename(path))
        if not m:
            continue
        dyn, string = m.group(1), m.group(2)
        x, rate = sf.read(path, always_2d=True)
        if rate == 96000:  # (Iowa's 2012 files: halved, the piano tool's analysis sizes)
            from scipy.signal import resample_poly
            x, rate = resample_poly(x, 1, 2, axis=0), 48000
        x = highpass(x, rate)
        starts = onsets(x, rate)
        for i, s in enumerate(starts):
            e = starts[i + 1] if i + 1 < len(starts) else len(x)
            seg = x[max(0, s - int(0.05 * rate)):e - int(0.03 * rate)]
            if len(seg) < int(0.7 * rate):
                continue
            seg = seg[mp.onset(seg, rate):]
            f0 = pitch(seg, rate)
            midi = int(round(69 + 12 * np.log2(f0 / 440.0) + 0.4))
            fret = midi - OPEN[string]
            if fret < 0 or fret > 19:
                continue
            key = (midi, dyn)
            if key not in found or fret < found[key][0]:
                found[key] = (fret, string, seg, rate, f0)
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--iowa", help="folder of the Iowa guitar's stereo AIFF files")
    parser.add_argument("--out", default="content/guitar/modal-guitar.bin")
    args = parser.parse_args()
    if not args.iowa:
        sys.exit("give --iowa DIR")

    found = iowa_notes(args.iowa)
    notes = sorted({m for (m, d) in found if all((m, dd) in found for dd in DYNAMICS)})
    print("notes:", notes)
    rate = found[(notes[0], "mf")][3]

    # Board (body) modes and the stereo picture from each note's ff.
    modes, pictures = {}, {}
    for note in notes:
        _, _, seg, _, f0 = found[(note, "ff")]
        ratios, _, _ = analyse(seg, rate, f0)
        modes[note] = mp.body_modes(seg, rate, f0, ratios * f0)
        pictures[note] = (mp.stereo(seg, rate, ratios * f0), mp.stereo(seg, rate, np.array(modes[note])))

    mp.TIMES = TIMES  # (the mode and noise readers use the piano tool's grid)
    entries = []
    for dyn, velocity in DYNAMICS.items():
        for note in notes:
            fret, string, seg, _, f0 = found[(note, dyn)]
            ratios, levels, valid = analyse(seg, rate, f0)
            start = seg[:int(mp.ATTACK_SECONDS * rate)]
            attack = (start[:, 0], start[:, 1 if start.shape[1] > 1 else 0])
            (pb, pp), mpic = pictures[note]
            count = min(len(ratios), len(pb))
            ppic = (np.pad(pb[:count], (0, len(ratios) - count)), np.pad(pp[:count], (0, len(ratios) - count)))
            # (The mode and noise readers' longer window runs out sooner.)
            valid16 = [max(0, int(when * rate) - 8192) + 16384 <= len(seg) for when in TIMES]
            mlev = carry_on(mp.mode_levels(seg, rate, modes[note]), valid16)
            nlev = carry_on(mp.noise_levels(seg, rate, f0, ratios * f0, modes[note]), valid16)
            entries.append((note, velocity, f0, ratios, levels, attack, modes[note], mlev, nlev, ppic, mpic))
            print("note %d %s (%s fret %d, %.2f s, %+.0f ct): %d partials, %d body modes"
                  % (note, dyn, string, fret, len(seg) / rate, 1200 * np.log2(f0 / (440 * 2 ** ((note - 69) / 12))),
                     len(ratios), len(modes[note])), flush=True)

    out = bytearray(b"IMPT")
    out += struct.pack("<HHHf", 4, len(entries), len(TIMES), float(rate))
    out += struct.pack("<%df" % len(TIMES), *TIMES)
    for note, velocity, f0, ratios, levels, (attack, attackRight), bmodes, mlevels, nlevels, ppic, mpic in entries:
        scale = max(1e-9, float(np.abs(attack).max()))
        out += struct.pack("<HHHI", note, velocity, len(ratios), len(attack))
        out += struct.pack("<f", scale)
        out += struct.pack("<%df" % len(ratios), *ratios.astype(np.float32))
        out += np.clip(np.round(levels * 64), -32768, 32767).astype("<i2").tobytes()
        out += np.clip(np.round(attack / scale * 32767), -32767, 32767).astype("<i2").tobytes()
        out += struct.pack("<H", len(bmodes))
        out += struct.pack("<%df" % len(bmodes), *bmodes)
        out += np.clip(np.round(mlevels * 64), -32768, 32767).astype("<i2").tobytes()
        out += struct.pack("<H", len(mp.NOISE_BANDS))
        out += np.clip(np.round(nlevels * 64), -32768, 32767).astype("<i2").tobytes()
        right = max(1e-9, float(np.abs(attackRight).max()))
        out += struct.pack("<f", right)
        out += np.clip(np.round(attackRight / right * 32767), -32767, 32767).astype("<i2").tobytes()
        for balance, phase in (ppic, mpic):
            out += np.clip(np.round(balance * 64), -32768, 32767).astype("<i2").tobytes()
            out += np.clip(np.round(phase * 10000), -32768, 32767).astype("<i2").tobytes()
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(out)
    print("wrote %s (%d bytes, %d entries)" % (args.out, len(out), len(entries)))


if __name__ == "__main__":
    main()
