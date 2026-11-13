"""The picture score: how far a render's spectrogram looks from a recording's.

Band terms (1/6-octave bands, 80 Hz to 16 kHz, levels against the sustain's
mean): sustain (the average spectrum's shape), wobble (how much each band
moves over time), attack (0 to 0.6 s) and release (4.0 to 4.6 s, the note let
go at 4 s). Partial terms (each harmonic up to 10 kHz): plevel (the level
profile), vibam (how deep each partial's level moves at the vibrato's rate,
3 to 10 Hz) and slow (how much it drifts under 2 Hz), the last two over the
partials within 40 dB of the loudest. Lower is closer.
"""
import numpy as np
from scipy.signal import butter, sosfiltfilt, stft

SR = 48000
EDGES = np.geomspace(80, 16000, 49)  # 1/6-octave bands


def bandgram(x):
    f, t, Z = stft(x, SR, nperseg=2048, noverlap=1536)
    P = np.abs(Z) ** 2
    # (bands with no STFT bin in them, three at the bottom, are left out: an
    # empty band's level would only follow the render's overall loudness)
    B = np.array([P[(f >= a) & (f < b)].sum(0) for a, b in zip(EDGES, EDGES[1:]) if ((f >= a) & (f < b)).any()])
    return t, 10 * np.log10(B + 1e-14)


def band_parts(x):
    t, B = bandgram(x)
    B = B - B[:, (t > 1.5) & (t < 3.5)].mean()  # level reference: the sustain's mean
    S = B[:, (t > 1.2) & (t < 3.8)]
    return dict(sustain=S.mean(1), wobble=S.std(1), attack=B[:, (t > 0.0) & (t < 0.6)], release=B[:, (t > 4.0) & (t < 4.6)])


def partials(x, f0, top=10000):
    f, t, Z = stft(x, SR, nperseg=4096, noverlap=4096 - 240)
    P = np.abs(Z) ** 2
    dt = t[1] - t[0]
    keep = (t > 1.0) & (t < 3.8)
    fast = butter(2, [3, 10], "bandpass", fs=1 / dt, output="sos")
    slow = butter(2, 2, "lowpass", fs=1 / dt, output="sos")
    out = []
    for h in range(1, 200):
        fh = h * f0
        if fh > top:
            break
        k = (f > fh - 0.4 * f0) & (f < fh + 0.4 * f0)
        a = 10 * np.log10(P[k][:, keep].max(0) + 1e-14)
        out.append((fh, a.mean(), sosfiltfilt(fast, a).std(), sosfiltfilt(slow, a).std()))
    return np.array(out)


def dist(rec, syn, f0):
    a, b = band_parts(rec), band_parts(syn)
    fl = lambda v: np.maximum(v, -70)
    d = dict(sustain=np.mean(np.abs(fl(a["sustain"]) - fl(b["sustain"]))),
             wobble=np.mean(np.abs(a["wobble"] - b["wobble"])),
             attack=np.mean(np.abs(fl(a["attack"]) - fl(b["attack"][:, :a["attack"].shape[1]]))),
             release=np.mean(np.abs(fl(a["release"]) - fl(b["release"][:, :a["release"].shape[1]]))))
    A, B = partials(rec, f0), partials(syn, f0)
    c = min(len(A), len(B))
    A, B = A[:c], B[:c]
    la = np.maximum(A[:, 1] - A[:, 1].max(), -60)
    lb = np.maximum(B[:, 1] - B[:, 1].max(), -60)
    w = (la > -40).astype(float)
    w /= max(1, w.sum())
    d["plevel"] = np.mean(np.abs(la - lb))
    d["vibam"] = np.sum(w * np.abs(A[:, 2] - B[:, 2]))
    d["slow"] = np.sum(w * np.abs(A[:, 3] - B[:, 3]))
    d["vibam_rec"], d["vibam_syn"] = np.sum(w * A[:, 2]), np.sum(w * B[:, 2])
    d["slow_rec"], d["slow_syn"] = np.sum(w * A[:, 3]), np.sum(w * B[:, 3])
    d["total"] = d["sustain"] + d["wobble"] + 0.5 * d["attack"] + 0.5 * d["release"] + 0.5 * d["plevel"] + 2 * d["vibam"] + 2 * d["slow"]
    return d
