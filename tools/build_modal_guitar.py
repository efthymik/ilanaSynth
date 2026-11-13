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
  --render WAV --midi MID
                a render of a guitar plugin playing
                ilanasynth/modal-guitar-violin-research/render-midi/guitar-full-range.mid
                (E2..E6 one note at a time, velocities 40/80/120 = pp/mf/ff).

No recording is played back (2026-11-12): the pluck is modelled, from
format version 5 on (the engine: src/dsp/ModalPiano.h).
  - Four early times (0, 10, 20 and 35 ms) come before the piano tool's
    grid. At each one every partial's and body mode's level is fitted by
    least squares: their complex amplitudes at their known frequencies in
    a Hann window 2.5 periods of the note long (10..30 ms), with a little
    ridge, each one's basis falling at its own decay from 20 to 50 ms. The
    first time's window starts at the pluck and gives each one's level
    there and its phase (at the note's start, which the times count from).
    A fit is kept within 40 dB over and 30 dB under the 50 ms level (else
    that level: the upper partials of a pluck often start 20-40 dB over
    it); body modes closer to a partial, or to a louder mode, than the
    window can tell apart keep their 50 ms level.
  - A partial that has died away by 0.25 s (30 dB under the strongest)
    takes its frequency from the note's first frame: the late frame's peak
    near it was often the noise's, a few % off, and the fits missed it.
  - When the pluck starts after the note's start (as cut, about 2-5 ms)
    and how long it rises: the engine swells along one raised cosine from
    the note's start to the pluck's top (a rise whose 10-90 % time is the
    recording's, 0.5..4 ms, after the start), which also stands for the
    finger's noise before the string leaves it.
  - The pluck's noise: what the fitted sinusoids leave over its first 85 ms
    (fits every 5 ms, overlap-added), per noise band above 500 Hz, over the
    note's steady noise: its level (in the noise levels' measure, as at
    the note's start) and its fall in dB/s (at least enough to be under
    the steady noise 120 ms into the note).

Recorded notes are often cut short (Iowa's by the next note after 1 to 3 s):
each level curve past the end of its note carries on along the slope of its
last measured half second, so the table's time grid reaches 5.5 s for every
note. The partials' frequencies are kept as ratios to the note's measured
pitch (Iowa's guitar sits about 40 cents flat), so the engine plays in tune:
the median over the first six partials of f_k / (k sqrt(1 + B k^2)), not the
fundamental's peak, which the body pulls flat (round 4: 43, 44, 52 and 55
played 10 to 20 cents sharp). A render's entries carry the velocities its
MIDI played (40/80/120); Iowa's keep 35/80/120.

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

# The early times (the pluck, fitted: pluck()) and then the piano tool's grid.
EARLY = [0.0, 0.01, 0.02, 0.035]
TIMES = EARLY + [0.05, 0.08, 0.12, 0.2, 0.3, 0.45, 0.7, 1.0, 1.5, 2.0, 3.0, 4.0, 5.5]
AT_005, AT_008 = TIMES.index(0.05), TIMES.index(0.08)
VERSION = 5
# The modelled pluck (pluck()):
EARLY_OVER, EARLY_UNDER = 40.0, 30.0  # its fitted levels' bounds round the 50 ms one (dB)
W_PERIODS, W_MIN, W_MAX = 2.5, 0.01, 0.03  # its fits' window: periods of the note, within these seconds
RISE_SCALE = 1 / 0.6  # its rise's length over the recording's 10-90 % time (a raised cosine's 10-90 % is 0.6 of it)
BURST_FROM = 1  # the first noise band with a burst (under 500 Hz the fits leave the body's own boom)
EARLY_TRACK = 30.0  # dB under the strongest partial at 0.25 s from which a partial's frequency is the pluck's
OPEN = {"sulE": 40, "sulA": 45, "sulD": 50, "sulG": 55, "sulB": 59, "sul_E": 64}
DYNAMICS = {"pp": 35, "mf": 80, "ff": 120}  # (Iowa: velocities the takes are stored at)
RENDER_DYNAMICS = {"pp": 40, "mf": 80, "ff": 120}  # (a render: the velocities the MIDI played)


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
    """The note's f0: the median of f_k / (k sqrt(1 + B k^2)) over its first
    six partials (B fitted to the stretch of k >= 3). The
    fundamental's own peak is pulled off by the body (55 ff sat 17 ct under
    its harmonics' k x 196 Hz), and ratios to it played the note sharp."""
    seg = x[int(0.15 * rate):int(0.65 * rate)]
    seg = seg.mean(axis=1) if seg.ndim > 1 else seg
    n = 1 << 17
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    fr = np.fft.rfftfreq(n, 1 / rate)
    cands = np.arange(70, 1400, 0.25)
    score = [np.sum(np.interp(f0 * np.arange(1, 6), fr, sp)) for f0 in cands]
    f0 = cands[int(np.argmax(score))]
    found, amps = [], []
    for k in range(1, 9):
        near = np.where((fr > k * f0 * 0.97) & (fr < k * f0 * 1.03))[0]
        if len(near) == 0 or fr[near[-1]] > 0.45 * rate:
            break
        j = near[int(np.argmax(sp[near]))]
        if 0 < j < len(sp) - 1:  # (parabolic, in dB)
            a, b, c = 20 * np.log10(sp[j - 1:j + 2] + 1e-12)
            d = 0.5 * (a - c) / (a - 2 * b + c) if a - 2 * b + c < 0 else 0.0
            found.append((k, fr[j] + d * (fr[1] - fr[0])))
        else:
            found.append((k, fr[j]))
        amps.append(sp[j])
    if len(found) < 3:
        return float(found[0][1]) if found else float(f0)
    k = np.array([q for q, _ in found], float)
    f = np.array([q for _, q in found])
    # The partials within 40 dB of the loudest count alike (weighted by
    # level, the fundamental alone outweighed the rest on 43, 44 and 55,
    # and with the 2nd on 43/44 it is the one the body pulls off).
    db = 20 * np.log10(np.array(amps) + 1e-12)
    heard = db > db.max() - 40.0
    # (f_k / k)^2 = f0^2 (1 + B k^2): a line in k^2 (B >= 0), from k >= 3.
    upper = (k >= 3) & heard
    stretch = 0.0
    if upper.sum() >= 3:
        slope, intercept = np.polyfit(k[upper] ** 2, (f[upper] / k[upper]) ** 2, 1)
        stretch = max(0.0, slope / intercept) if intercept > 0 else 0.0
    use = (k <= 6) & heard
    return float(np.median((f / (k * np.sqrt(1 + stretch * k * k)))[use]))


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
    late = mp.spectrum(x, int(0.25 * rate), size)
    partials = mp.track(late, freqs, f0)
    half = max(2.0, 0.2 * f0)
    if len(partials):
        # A partial that has died away by 0.25 s: its frequency from the
        # pluck's frame, where it still shows (the late frame's peak near
        # it is often the noise's, a few % off).
        early = mp.track(mp.spectrum(x, 0, size), freqs, f0)
        peak = np.array([late[(freqs > f - half) & (freqs < f + half)].max() for f in partials])
        weak = 20 * np.log10(peak / peak.max() + 1e-12) < -EARLY_TRACK
        for n in range(min(len(partials), len(early))):
            if weak[n]:
                partials[n] = early[n]
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


def pluck_onset(x, rate):
    """(start, rise) in seconds: a raised cosine of length `rise` from
    `start` whose 10-90 % points sit on the recording's. The pluck is the
    steepest rise of the envelope (0.25 ms smoothed Hilbert) 1 to 10 ms in
    (mp.onset leaves it about 4 ms in; the first ms is the transform's
    edge); its top the most of the next 3 ms, its 10 % searched back."""
    from scipy.signal import hilbert
    m = x.mean(axis=1)[:int(0.05 * rate)]
    n = max(1, int(0.00025 * rate))
    e = np.convolve(np.abs(hilbert(m)), np.ones(n) / n, "same")
    db = 20 * np.log10(e + 1e-12)
    s = max(1, int(0.001 * rate))
    a, b = int(0.001 * rate), int(0.010 * rate)
    ts = a + int(np.argmax(db[a + s:b + s] - db[a:b])) + s // 2
    top = e[ts:ts + int(0.003 * rate)].max()
    t90 = ts + int(np.argmax(e[ts:] >= 0.9 * top))
    below = np.where(e[a:ts + 1] < 0.1 * top)[0]
    t10 = a + int(below[-1]) if len(below) else a
    rise = float(np.clip((t90 - t10) / rate * RISE_SCALE, 0.0005, 0.004))
    return max(0.0, t10 / rate - 0.2 * rise), rise


def fit_sines(x, rate, lo, hi, freqs, damp, ref, ridge=1e-4):
    """Hann-weighted ridge least squares of x[lo:hi] (every channel) on
    exp(-damp (t - ref)) {sin, cos}(2 pi f t), t from the segment's start:
    (sin and cos coefficients [freqs x channels], the fitted signal)."""
    t = np.arange(lo, hi) / rate
    w = np.hanning(hi - lo + 2)[1:-1]
    env = np.exp(-np.outer(t - ref, damp))
    arg = 2 * np.pi * np.outer(t, freqs)
    B = np.concatenate([env * np.sin(arg), env * np.cos(arg)], axis=1)
    G = (B * w[:, None]).T @ B
    G[np.diag_indices_from(G)] += ridge * np.mean(np.diag(G)) + 1e-30
    c = np.linalg.solve(G, (B * w[:, None]).T @ x[lo:hi])
    return c[:len(freqs)], c[len(freqs):], B @ c


def noise_variance(db, band, rate):
    """A noise level (mp.noise_levels' median bin, dB) as the band's variance."""
    lo, hi = mp.NOISE_BANDS[band]
    return 10 ** (db / 10) * 16384 / 6 / np.log(2) * 2 * (hi - lo) / rate


def pluck(x, rate, f0, partials, modes, levels, mlevels, nlevels, onset):
    """The modelled pluck (see the top): fills levels' and mlevels' early
    columns in place and returns (start s, rise s, partial phases, mode
    phases (radians at the segment's start, NaN: not fitted), burst dB
    [bands] (the noise levels' measure), burst fall dB/s [bands])."""
    start, rise = onset
    W = float(np.clip(W_PERIODS / f0, W_MIN, W_MAX))
    top = min(16000.0, 0.45 * rate)
    use = [i for i, f in enumerate(partials) if f < top]
    near = 0.7 / W  # (a body mode this close to a partial can't be told from it)
    kept = []  # (the loudest first, of modes too close to tell apart)
    for m in sorted(range(len(modes)), key=lambda m: -mlevels[m, AT_005]):
        f = modes[m]
        if f > 20 and all(abs(f - partials[i]) > near for i in use) and all(abs(f - modes[j]) > near for j in kept):
            kept.append(m)
    kept.sort()
    freqs = np.concatenate([np.asarray(partials, float)[use], np.asarray(modes, float)[kept]])
    rows = [("p", i) for i in use] + [("m", m) for m in kept]
    total = len(x)

    def window(t):
        lo = int(round(max(start, t - W / 2) * rate))
        return lo, min(total, int(round(max(t + W / 2, start + W / 2) * rate)))

    def amplitude(a, b):
        return np.sqrt((a ** 2 + b ** 2).mean(axis=1))  # (the channels' power average)

    # Each one's decay from 20 to 50 ms (undamped fits), then the early times.
    fitted = {}
    for t in (0.02, 0.05):
        a, b, _ = fit_sines(x, rate, *window(t), freqs, np.zeros(len(freqs)), t)
        fitted[t] = amplitude(a, b)
    damp = np.clip(np.log((fitted[0.02] + 1e-12) / (fitted[0.05] + 1e-12)) / 0.03, 0.0, 2.0 / W)
    phase = np.full(len(freqs), np.nan)
    for j, t in enumerate(EARLY):
        if t == 0.0:
            lo, hi, ref = int(round(start * rate)), int(round((start + W) * rate)), start
        else:
            (lo, hi), ref = window(t), t
        a, b, _ = fit_sines(x, rate, lo, hi, freqs, damp, ref)
        amp = amplitude(a, b)
        if t == 0.0:
            phase = np.arctan2(b[:, 0], a[:, 0])  # (a sin + b cos = A sin (wt + phase), the left channel's)
        for r, (kind, i) in enumerate(rows):
            rowset = levels if kind == "p" else mlevels
            was = rowset[i, AT_005]
            db = 20 * np.log10(amp[r]) if amp[r] > 0 and np.isfinite(amp[r]) else was
            # Within EARLY_OVER dB over the 50 ms level and EARLY_UNDER under it
            # (a fit gone wrong); a failed one keeps the 50 ms level.
            rowset[i, j] = float(np.clip(db, was - EARLY_UNDER, was + EARLY_OVER))
    for rowset, fitted_rows in ((levels, set(use)), (mlevels, set(kept))):
        for i in range(len(rowset)):
            if i not in fitted_rows:
                rowset[i, :AT_005] = rowset[i, AT_005]
    partial_phase = np.full(len(partials), np.nan)
    mode_phase = np.full(len(modes), np.nan)
    for r, (kind, i) in enumerate(rows):
        (partial_phase if kind == "p" else mode_phase)[i] = phase[r]

    # The burst: the fitted sinusoids (fits every 5 ms, overlap-added)
    # taken from the first 85 ms, per band, over the steady noise.
    step, span, a0 = 0.005, int(0.085 * rate), int(round(start * rate))
    model = np.zeros((span, x.shape[1]))
    weight = np.zeros(span)
    for c in np.arange(start, start + 0.085 + step, step):
        lo, hi = window(c)
        _, _, fitted_signal = fit_sines(x, rate, lo, hi, freqs, damp, c)
        n = np.arange(lo, hi)
        tri = np.clip(1 - np.abs(n / rate - c) / step, 0, 1)
        n -= a0
        ok = (n >= 0) & (n < span)
        model[n[ok]] += tri[ok, None] * fitted_signal[ok]
        weight[n[ok]] += tri[ok]
    rest = x[a0:a0 + span] - model / np.maximum(weight, 1e-9)[:, None]
    size, hop = 256, 128
    win = np.hanning(size)
    freq = np.fft.rfftfreq(size, 1 / rate)
    frames = [(i + size / 2) / rate for i in range(0, len(rest) - size + 1, hop)]
    spectra = [(np.abs(np.fft.rfft(rest[i:i + size] * win[:, None], axis=0)) ** 2).mean(axis=1)
               for i in range(0, len(rest) - size + 1, hop)]
    burst = np.full(len(mp.NOISE_BANDS), -160.0)
    fall = np.zeros(len(mp.NOISE_BANDS))
    for band in range(BURST_FROM, len(mp.NOISE_BANDS)):
        lo, hi = mp.NOISE_BANDS[band]
        k = (freq >= lo) & (freq < min(hi, rate / 2))
        if not k.any():
            continue
        steady = noise_variance(nlevels[band, 0], band, rate)
        var = np.array([2 * sp[k].sum() / (size * (win ** 2).sum()) for sp in spectra])
        excess = var - steady
        t = np.array(frames)
        good = (excess > 0.1 * steady) & (t < 0.06)
        if good.sum() < 2:
            continue
        y = 10 * np.log10(excess[good])
        slope, at0 = np.polyfit(t[good], y, 1, w=np.sqrt(excess[good] / excess[good].max()))
        # Its level at the pluck (dB of variance), falling at least fast
        # enough to be under the steady noise 120 ms into the note; kept as
        # at the note's start (the engine's burst falls from there).
        steady_db = 10 * np.log10(steady + 1e-30)
        decay = max(float(np.clip(-slope, 30.0, 4000.0)), (at0 - steady_db) / (0.12 - start))
        level = at0 + decay * start
        # Back to the noise levels' measure (the median bin's dB).
        burst[band] = float(level - steady_db + nlevels[band, 0])
        fall[band] = decay
    return start, rise, partial_phase, mode_phase, burst, fall


def iowa_notes(folder):
    """{(midi, dynamic): (fret, string, segment, rate, f0)}: each note from the string where its fret is lowest, of the takes that are sound."""
    takes = {}
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
            takes.setdefault((midi, dyn), []).append((fret, string, seg, rate, f0))
    # Each note from the lowest fret whose take is sound (2026-11-11: some
    # Iowa takes have a stretch of digital silence just after the pluck, an
    # edit in the source file, so the table had E3 pp and F3 mf 60-70 dB
    # quiet; A#4 pp's pitch read lands between partials).
    found = {}
    for key, options in takes.items():
        options.sort(key=lambda o: o[0])
        good = [o for o in options if sound_take(o[2], o[3], o[4])]
        if not good:
            # The pitch read missed: the note's pitch from its other dynamics.
            others = [o[4] for (m, d), more in takes.items() if m == key[0] and d != key[1] for o in more]
            if others:
                f0 = float(np.median(others))
                good = [o[:4] + (f0,) for o in options if sound_take(o[2], o[3], f0)]
        found[key] = good[0] if good else options[0]
        if not good or good[0][:2] != options[0][:2] or good[0][4] != options[0][4]:
            print("note %d %s: %s fret %d, %.1f Hz (first take %s fret %d, %.1f Hz)%s"
                  % (key[0], key[1], found[key][1], found[key][0], found[key][4], options[0][1], options[0][0],
                     options[0][4], "" if good else ": NO SOUND TAKE"), flush=True)
    return found


def tame_tails(entries, ref=AT_008):
    """Body modes and noise fall at least as fast as the note's partials
    after `ref` (0.08 s). (2026-11-11: Iowa's takes are cut by the next
    note and other strings ring on, so late in many notes the "modes"
    and "noise" were 10-30 dB over the string itself: a hiss or a stray
    pitch in some notes' tails.)"""
    out = []
    for note, vel, f0, ratios, levels, plucked, bmodes, mlevels, nlevels, ppic, mpic in entries:
        energy = 10 * np.log10(np.sum(10 ** (levels / 10), axis=0) + 1e-30)
        fall = np.minimum(0.0, energy - energy[ref])
        def tame(rows):
            if len(rows) == 0:
                return rows
            cap = rows[:, ref:ref + 1] + fall[None, :]
            out_rows = rows.copy()
            out_rows[:, ref:] = np.minimum(rows[:, ref:], cap[:, ref:])
            return np.maximum(-160.0, out_rows)
        out.append((note, vel, f0, ratios, levels, plucked, bmodes, tame(mlevels), tame(nlevels), ppic, mpic))
    return out



def sound_take(seg, rate, f0):
    """False for a take with a dropout (5 ms of digital silence) in its
    first second, or whose first partial is not near its pitch."""
    hop = int(0.005 * rate)
    head = seg[:int(1.0 * rate)].mean(axis=1)
    frames = len(head) // hop
    db = 10 * np.log10((head[:frames * hop].reshape(frames, hop) ** 2).mean(axis=1) + 1e-30)
    if (db < -120).any():
        return False
    ratios, _, _ = analyse(seg, rate, f0)
    return len(ratios) > 0 and abs(ratios[0] - 1.0) < 0.03


def even_levels(entries, reach=3, allowed=3.0):
    """Each dynamic layer's notes pulled to within `allowed` dB of the
    median of their neighbours (`reach` notes each side), all of a note's
    levels and its pluck's noise together, so its timbre stays as recorded.
    (2026-11-11: Iowa's takes jump 10-20 dB from note to note, e.g. the
    open E4 and C5, which then read as a quiet, thin sine next to their
    neighbours.)"""
    out = list(entries)
    t = AT_008  # (0.08 s: past the pluck)
    for velocity in sorted({e[1] for e in entries}):
        index = [i for i, e in enumerate(entries) if e[1] == velocity]
        index.sort(key=lambda i: entries[i][0])
        energy = np.array([10 * np.log10(np.sum(10 ** (entries[i][4][:, t] / 10)) + 1e-30) for i in index])
        for j, i in enumerate(index):
            near = np.median(energy[max(0, j - reach):j + reach + 1])
            shift = float(np.clip(energy[j], near - allowed, near + allowed) - energy[j])
            if abs(shift) < 1e-3:
                continue
            note, vel, f0, ratios, levels, (start, rise, pphase, mphase, burst, fall), bmodes, mlevels, nlevels, ppic, mpic = entries[i]
            plucked = (start, rise, pphase, mphase, np.maximum(-160.0, burst + shift), fall)
            out[i] = (note, vel, f0, ratios, np.maximum(-160.0, levels + shift), plucked, bmodes,
                      np.maximum(-160.0, mlevels + shift), np.maximum(-160.0, nlevels + shift), ppic, mpic)
            print("note %d v%d: %+.1f dB (to its neighbours)" % (note, vel, shift), flush=True)
    return out


def render_notes(path, midi_path):
    """{(midi, dynamic): (0, "render", segment, rate, f0)} from a render of a
    MIDI file (one note at a time; velocities 40/80/120 are pp/mf/ff)."""
    import mido
    x, rate = sf.read(path, always_2d=True)
    x = highpass(x, rate)
    events, t, held = [], 0.0, {}
    for msg in mido.MidiFile(midi_path):
        t += msg.time
        if msg.type == "note_on" and msg.velocity > 0:
            held[msg.note] = (t, msg.velocity)
        elif msg.type in ("note_off", "note_on") and msg.note in held:
            start, velocity = held.pop(msg.note)
            events.append((start, t, msg.note, velocity))
    level = {v: d for d, v in RENDER_DYNAMICS.items()}
    found = {}
    for start, end, note, velocity in events:
        seg = x[int(start * rate):min(len(x), int((end + 0.9) * rate))]
        seg = seg[mp.onset(seg, rate):]
        found[(note, level[velocity])] = (0, "render", seg, rate, pitch(seg, rate))
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--iowa", help="folder of the Iowa guitar's stereo AIFF files")
    parser.add_argument("--render", help="a render of guitar-full-range.mid (WAV or FLAC)")
    parser.add_argument("--midi", help="the MIDI file the render played")
    parser.add_argument("--out", default="content/guitar/modal-guitar.bin")
    args = parser.parse_args()
    dynamics = DYNAMICS
    if args.render and args.midi:
        found = render_notes(args.render, args.midi)
        dynamics = RENDER_DYNAMICS
    elif args.iowa:
        found = iowa_notes(args.iowa)
    else:
        sys.exit("give --iowa DIR, or --render WAV --midi MID")
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
    for dyn, velocity in dynamics.items():
        for note in notes:
            fret, string, seg, _, f0 = found[(note, dyn)]
            onset = pluck_onset(seg, rate)
            ratios, levels, valid = analyse(seg, rate, f0)
            (pb, pp), mpic = pictures[note]
            count = min(len(ratios), len(pb))
            ppic = (np.pad(pb[:count], (0, len(ratios) - count)), np.pad(pp[:count], (0, len(ratios) - count)))
            # (The mode and noise readers' longer window runs out sooner.)
            valid16 = [max(0, int(when * rate) - 8192) + 16384 <= len(seg) for when in TIMES]
            mlev = carry_on(mp.mode_levels(seg, rate, modes[note]), valid16)
            nlev = carry_on(mp.noise_levels(seg, rate, f0, ratios * f0, modes[note]), valid16)
            plucked = pluck(seg, rate, f0, ratios * f0, modes[note], levels, mlev, nlev, onset)
            entries.append((note, velocity, f0, ratios, levels, plucked, modes[note], mlev, nlev, ppic, mpic))
            print("note %d %s (%s fret %d, %.2f s, %+.0f ct): %d partials, %d body modes, pluck at %.1f ms, rise %.1f ms"
                  % (note, dyn, string, fret, len(seg) / rate, 1200 * np.log2(f0 / (440 * 2 ** ((note - 69) / 12))),
                     len(ratios), len(modes[note]), plucked[0] * 1e3, plucked[1] * 1e3), flush=True)

    if args.iowa:
        entries = even_levels(entries)
        entries = tame_tails(entries)

    # The Modal Piano's format (build_modal_piano.py), version 5: no attack
    # samples, and after the stereo picture the pluck (pluck()).
    out = bytearray(b"IMPT")
    out += struct.pack("<HHHf", VERSION, len(entries), len(TIMES), float(rate))
    out += struct.pack("<%df" % len(TIMES), *TIMES)
    def phases(values):
        return np.where(np.isfinite(values), np.clip(np.round(np.nan_to_num(values) * 10000), -32767, 32767), -32768).astype("<i2").tobytes()
    for note, velocity, f0, ratios, levels, plucked, bmodes, mlevels, nlevels, ppic, mpic in entries:
        start, rise, pphase, mphase, burst, fall = plucked
        out += struct.pack("<HHHI", note, velocity, len(ratios), 0)  # (no attack samples)
        out += struct.pack("<f", 1.0)
        out += struct.pack("<%df" % len(ratios), *ratios.astype(np.float32))
        out += np.clip(np.round(levels * 64), -32768, 32767).astype("<i2").tobytes()
        out += struct.pack("<H", len(bmodes))
        out += struct.pack("<%df" % len(bmodes), *bmodes)
        out += np.clip(np.round(mlevels * 64), -32768, 32767).astype("<i2").tobytes()
        out += struct.pack("<H", len(mp.NOISE_BANDS))
        out += np.clip(np.round(nlevels * 64), -32768, 32767).astype("<i2").tobytes()
        out += struct.pack("<f", 1.0)  # (the right channel's attack: none)
        for balance, phase in (ppic, mpic):
            out += np.clip(np.round(balance * 64), -32768, 32767).astype("<i2").tobytes()
            out += np.clip(np.round(phase * 10000), -32768, 32767).astype("<i2").tobytes()
        # Version 5: the pluck's start and rise (ms), each partial's and
        # mode's phase at the start of the note (1/10000 rad; -32768: none,
        # a random one), and the burst per band (dB, the noise levels'
        # measure, at the pluck; then dB/s).
        out += struct.pack("<ff", start * 1e3, rise * 1e3)
        out += phases(pphase)
        out += phases(mphase)
        out += struct.pack("<%df" % len(mp.NOISE_BANDS), *burst)
        out += struct.pack("<%df" % len(mp.NOISE_BANDS), *fall)
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(out)
    print("wrote %s (%d bytes, %d entries)" % (args.out, len(out), len(entries)))


if __name__ == "__main__":
    main()
