"""Python twin of src/dsp/ModalBowed.h (bowed notes) for fitting (DEFAULT: the engine as
fitted). The release: lift=1 keeps the bow's speed as its pressure eases over rel_s
(lift=0, the bow slowing to a stop on the string, braked the note into a cliff about
0.35 s after the note-off), the upper partials' losses rise to ring times over ring_s
and fade_db fades the note (dB a second) once let go."""
import os
import re
import numpy as np
from numba import njit

SR = 48000
H = open(os.environ.get("BODIES", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "dsp", "ModalBowedBodies.h"))).read()


def arr(name):
    m = re.search(r"float %s\[count\] \{([^}]*)\}" % name, H)
    return np.array([float(v.strip().rstrip("f")) for v in m.group(1).split(",")])


BODY = {"hz": arr("hz"), "bw": arr("bandwidth"), "violin": arr("violin"), "viola": arr("violas"), "cello": arr("cellos")}
SIZES = {"violin": 1.0, "viola": 1.17, "cello": 2.1}
DB = 0.1151292546  # dB to a natural-log factor


@njit(cache=True)
def drift_gain(sr, hz, std):
    # Two one-poles in a row on uniform noise at the block rate (smooth: no
    # fast jitter to blur the partials): the pole for a corner of hz and the
    # gain that gives the wanted standard deviation.
    a = 1.0 - np.exp(-2.0 * np.pi * hz * 32.0 / sr)
    p = 1.0 - a
    return a, std / np.sqrt(a ** 4 * (1.0 + p * p) / (1.0 - p * p) ** 3 / 3.0)


@njit(cache=True)
def kernel(f0, total, held, sr, modes, beta, loss0, loss1, b, grip, vbow, vel, seed,
           vib_ct, vib_rate, vib_delay, vib_rise, attack_s, swell_s, swell_amt,
           rosin, jitter_amt, rel_s, fade_db,
           bz_b0, bz_a1, bz_a2, bz_g, outgain, hiss, hiss_hp, lp_hz,
           size, rip_db, rip_c, slow_db, slow_hz, loud_db, vib_rate_dev, vib_depth_dev, vib_drift_hz, direct, ring, lift, ring_s,
           fl_db, fl_hz, fl_slope, fl_hzslope, jit_ct, jit_hz, pdrift_ct, pdrift_hz,
           fl_knee, fl_fslope, rip_knee, rip_fslope, direct_hp, fl_damp, fl_clip):
    k = 1.0 / sr
    n_ = np.arange(1, modes + 1).astype(np.float64)
    shape = np.sin(n_ * np.pi * beta)
    push = 2.0 * k * shape / max(1e-6, np.sum(shape * shape))
    out0 = n_ / modes
    out = out0.copy()
    out_step = np.zeros(modes)
    hz0 = np.minimum(n_ * f0 * np.sqrt(1.0 + b * n_ * n_), sr * 0.49)
    # The body's fine ripple: two constant-Q sines in log frequency (the
    # body's dense resonances, scaled with its size); a partial the vibrato
    # sweeps across them rises and falls.
    oct_ = np.log2(hz0 * size / 1000.0)
    ph1 = 2 * np.pi * rip_c * oct_
    ph2 = 2 * np.pi * rip_c * 1.618034 * oct_ + 1.3
    base1 = np.sin(ph1); base2 = np.sin(ph2)
    # How deep each partial's ripple and flutter go: the body's resonances
    # thin out low down (little pulsing on a low partial), and the hair
    # shakes the upper partials more than the low ones.
    rshape = np.minimum(1.0, (hz0 * size / rip_knee) ** rip_fslope)
    fshape = np.minimum(1.0, (hz0 / fl_knee) ** fl_fslope)
    sigma = loss0 + loss1 * hz0 * hz0
    decay = np.exp(-sigma * k)
    turn = 2.0 * decay * np.cos(2 * np.pi * hz0 * k)
    fall = decay * decay
    q = np.zeros(modes); qp = np.zeros(modes)
    nb = len(bz_b0)
    bx1 = np.zeros(nb); bx2 = np.zeros(nb); by1 = np.zeros(nb); by2 = np.zeros(nb)
    y = np.zeros(total)
    ac = 1.0 - np.exp(-1.0 / (attack_s * sr))
    rc = 1.0 - np.exp(-1.0 / (rel_s * sr))
    bite_fall = np.exp(-1.0 / (0.08 * sr))
    env = 0.0; bite = 1.0; s = seed; swell = 0.0; senv = 0.0
    sc = 1.0 - np.exp(-1.0 / (max(1e-3, swell_s) * sr))
    hp_state = 0.0; hp_prev = 0.0
    hp_a = np.exp(-2 * np.pi * hiss_hp / sr)
    phase = 0.0; vib_env = 0.0
    lp_a = np.exp(-2 * np.pi * lp_hz / sr) if lp_hz < sr * 0.45 else 0.0
    lp = 0.0
    level = 1.0; fade = 10 ** (-fade_db / 20 / sr)
    # The box radiates little below its lowest resonance: the direct path is
    # high-passed (two poles, Butterworth) at direct_hp / size.
    use_hp = direct_hp > 0.0
    hb0 = 1.0; hb1 = 0.0; hb2 = 0.0; ha1 = 0.0; ha2 = 0.0
    if use_hp:
        w0 = 2 * np.pi * min(direct_hp / size, sr * 0.45) / sr
        al = np.sin(w0) / (2 * 0.7071067811865476)
        cw = np.cos(w0); a0 = 1 + al
        hb0 = (1 + cw) / 2 / a0; hb1 = -(1 + cw) / a0; hb2 = hb0; ha1 = -2 * cw / a0; ha2 = (1 - al) / a0
    hx1 = 0.0; hx2 = 0.0; hy1 = 0.0; hy2 = 0.0
    # The slow wander: each partial's level, the bow's speed, and the
    # vibrato's rate and depth (a second noise stream, so the audio noise
    # stays as it was).
    d = s ^ 0x9E3779B9
    sa, sg = drift_gain(sr, slow_hz, slow_db)
    _, lg = drift_gain(sr, slow_hz, loud_db)
    va, vg_r = drift_gain(sr, vib_drift_hz, vib_rate_dev)
    _, vg_d = drift_gain(sr, vib_drift_hz, vib_depth_dev)
    wander = np.zeros(modes); wander1 = np.zeros(modes); loud = 0.0; loud1 = 0.0
    vr = 0.0; vr1 = 0.0; vd = 0.0; vd1 = 0.0
    # The bow's fast randomness (a third noise stream): each partial's level
    # flutters on its own (fl_db at about fl_hz), the pitch jitters (jit_ct at
    # about jit_hz) and drifts slowly (the player's intonation, pdrift_ct).
    e = s ^ 0x85EBCA6B
    fa, fg = drift_gain(sr, min(100.0, fl_hz * (f0 / 440.0) ** fl_hzslope), fl_db * (f0 / 440.0) ** fl_slope)
    ja, jg = drift_gain(sr, jit_hz, jit_ct)
    pa, pg = drift_gain(sr, pdrift_hz, pdrift_ct)
    flut = np.zeros(modes); flut1 = np.zeros(modes)
    jit = 0.0; jit1 = 0.0; pdr = 0.0; pdr1 = 0.0
    loud_f = 1.0
    for i in range(total):
        if i % 32 == 0 and i >= held:
            # Let go: the upper partials' losses rise to ring times over
            # ring_s (the lifting bow's hair stops feeding them).
            gone = min(1.0, (i - held) / (max(1e-3, ring_s) * sr))
            sigma = loss0 + loss1 * (1.0 + (ring - 1.0) * gone) * hz0 * hz0
            decay = np.exp(-sigma * k)
            fall = decay * decay
        if i % 32 == 0:
            t = i / sr
            if t > vib_delay:
                vib_env = min(1.0, (t - vib_delay) / max(1e-3, vib_rise))
            for m in range(modes):
                d = (d * 1664525 + 1013904223) % 4294967296
                wander1[m] += sa * (sg * ((d >> 8) * (2.0 / 16777216.0) - 1.0) - wander1[m])
                wander[m] += sa * (wander1[m] - wander[m])
            d = (d * 1664525 + 1013904223) % 4294967296
            loud1 += sa * (lg * ((d >> 8) * (2.0 / 16777216.0) - 1.0) - loud1)
            loud += sa * (loud1 - loud)
            d = (d * 1664525 + 1013904223) % 4294967296
            vr1 += va * (vg_r * ((d >> 8) * (2.0 / 16777216.0) - 1.0) - vr1)
            vr += va * (vr1 - vr)
            d = (d * 1664525 + 1013904223) % 4294967296
            vd1 += va * (vg_d * ((d >> 8) * (2.0 / 16777216.0) - 1.0) - vd1)
            vd += va * (vd1 - vd)
            for m in range(modes):
                e = (e * 1664525 + 1013904223) % 4294967296
                flut1[m] += fa * (fg * ((e >> 8) * (2.0 / 16777216.0) - 1.0) - flut1[m])
                flut[m] += fa * (flut1[m] - flut[m])
            e = (e * 1664525 + 1013904223) % 4294967296
            jit1 += ja * (jg * ((e >> 8) * (2.0 / 16777216.0) - 1.0) - jit1)
            jit += ja * (jit1 - jit)
            e = (e * 1664525 + 1013904223) % 4294967296
            pdr1 += pa * (pg * ((e >> 8) * (2.0 / 16777216.0) - 1.0) - pdr1)
            pdr += pa * (pdr1 - pdr)
            loud_f = np.exp(DB * loud)
            phase += 2 * np.pi * vib_rate * (1.0 + vr) * 32.0 / sr
            if phase > 2 * np.pi:
                phase -= 2 * np.pi
            cents = vib_ct * (1.0 + vd) * vib_env * np.sin(phase) + jit + pdr
            ratio = 2.0 ** (cents / 1200.0)
            hz = np.minimum(hz0 * ratio, sr * 0.49)
            if fl_damp > 0:
                # The flutter as the hair's grip on each partial: its loss
                # wavers (a driven partial's level goes as 1 / loss), so only
                # the partial's own ring moves, at no faster than it can.
                # (with the hair on the string: it eases off as the bow lifts)
                # (held within about fl_clip dB either way: an unbounded swing
                # would choke a partial for a moment, a dropout)
                swing_db = fl_clip * np.tanh(env * flut * fshape / fl_clip)
                dk = np.exp(-sigma * np.exp(-DB * swing_db) * k)
                fall = dk * dk
                turn = 2.0 * dk * np.cos(2 * np.pi * hz * k)
            else:
                turn = 2.0 * decay * np.cos(2 * np.pi * hz * k)
            swing = 2 * np.pi * rip_c * cents / 1200.0
            ripple = 0.5 * rip_db * rshape * (np.sin(ph1 + swing) - base1 + np.sin(ph2 + 1.618034 * swing) - base2)
            # (glided to over the block: a step would zip)
            out_step = (out0 * np.exp(DB * (ripple + wander + (0.0 if fl_damp > 0 else 1.0) * flut * fshape)) - out) / 32.0
        nxt = turn * q - fall * qp
        v = np.sum(shape * (nxt - qp)) * 0.5 * sr
        on = 1.0 if i < held else 0.0
        env += (on - env) * (ac if on > 0 else rc)
        swell += (on - swell) * sc
        # The bow's speed: lift > 0 keeps it going as the bow leaves the
        # string (a lift, not a stop that brakes the string).
        fac = env * (1.0 - swell_amt + swell_amt * swell)
        if on > 0:
            senv = fac
        else:
            senv = fac + lift * (senv - fac)
        s = (s * 1664525 + 1013904223) % 4294967296
        w1 = (s >> 8) * (2.0 / 16777216.0) - 1.0
        s = (s * 1664525 + 1013904223) % 4294967296
        w2 = (s >> 8) * (2.0 / 16777216.0) - 1.0
        force = 0.0
        noise_out = 0.0
        if env > 1e-5:
            speed = vbow * loud_f * senv
            bow = speed * (1.0 + 0.6 * vel * bite) * (1.0 + w1 * jitter_amt)
            dv = bow - v
            slope = 5.0 - 4.0 * grip
            table = min(1.0, (abs(dv * slope) + 0.75) ** -4.0)
            force = dv * table * env
            noise_out = w2 * vbow * loud_f * fac
        bite *= bite_fall
        nxt = nxt + push * force
        qp = q; q = nxt
        out = out + out_step
        if i >= held:
            level *= fade
        bridge = (np.sum(out * q) + rosin * noise_out * 1e-4) * level
        dr = bridge
        if use_hp:
            dr = hb0 * bridge + hb1 * hx1 + hb2 * hx2 - ha1 * hy1 - ha2 * hy2
            hx2 = hx1; hx1 = bridge; hy2 = hy1; hy1 = dr
        snd = direct * dr
        for j in range(nb):
            yy = bz_b0[j] * (bridge - bx2[j]) - bz_a1[j] * by1[j] - bz_a2[j] * by2[j]
            bx2[j] = bx1[j]; bx1[j] = bridge; by2[j] = by1[j]; by1[j] = yy
            snd += bz_g[j] * yy
        # hiss: high-passed noise straight to the output (bow hair on the string)
        hp = hp_a * (hp_state + noise_out - hp_prev)
        hp_prev = noise_out; hp_state = hp
        raw = snd * outgain + hiss * hp
        lp = raw + lp_a * (lp - raw)
        y[i] = lp
    return y


def body_coeffs(inst, size=None):
    size = SIZES[inst] if size is None else size
    g = BODY[inst]
    b0, a1, a2, gg = [], [], [], []
    for hz, bw, db in zip(BODY["hz"], BODY["bw"], g):
        f = hz / size; w = bw / size
        if f >= SR * 0.45:
            continue
        r = np.exp(-np.pi * w / SR)
        b0.append((1 - r * r) * 0.5); a1.append(-2 * r * np.cos(2 * np.pi * f / SR)); a2.append(r * r); gg.append(10 ** (db / 20))
    return [np.array(v) for v in (b0, a1, a2, gg)]


DEFAULT = dict(beta=0.11, loss0=0.8, loss1=3.947e-06, bright=2.144, b=1.0e-5, grip=0.5, speed=0.5, vel=80 / 127,
               vib_ct=9.0, vib_rate=5.2, vib_delay=0.25, vib_rise=0.4, swell_s=0.34, swell_amt=0.97,
               rosin=0.01572, jitter=0.08536, rel_s=1.0, fade_db=36.96, hiss=0.0003336, hiss_hp=8000.0, top=12000.0,
               attack_scale=0.05, lp_hz=5590.0,
               rip_db=8.507, rip_c=19.46, slow_db=2.052, slow_hz=0.6, loud_db=1.742,
               vib_rate_dev=0.09024, vib_depth_dev=0.4561, vib_drift_hz=0.8, direct_db=-10.33, ring=16.05, lift=1.0, ring_s=0.494,
               fl_db=19.97, fl_hz=16.51, fl_slope=0.0, fl_hzslope=0.7853, jit_ct=0.0, jit_hz=20.0, pdrift_ct=1.441, pdrift_hz=0.5,
               fl_knee=818.2, fl_fslope=1.285, rip_knee=963.9, rip_fslope=2.074, direct_hp=152.0, fl_damp=1.0, fl_clip=30.0)


def play(inst, note, seconds=4.8, held=4.0, seed=1, **kw):
    p = dict(DEFAULT); p.update(kw)
    f0 = 440 * 2 ** ((note - 69) / 12)
    modes = int(max(1, min(64, min(p["top"], SR * 0.45) / f0)))
    attack = 0.05 * min(2.5, max(1.0, 220.0 / f0)) * p["attack_scale"]
    # The register's brightness: the upper strings lose less up high.
    loss1 = p["loss1"] * (f0 / 440.0) ** -p["bright"]
    b0, a1, a2, g = body_coeffs(inst)
    return kernel(f0, int(seconds * SR), int(held * SR), SR, modes, p["beta"], p["loss0"], loss1, p["b"], p["grip"],
                  0.05 + 0.3 * p["speed"], p["vel"], seed, p["vib_ct"], p["vib_rate"], p["vib_delay"], p["vib_rise"],
                  attack, p["swell_s"], p["swell_amt"], p["rosin"], p["jitter"], p["rel_s"], p["fade_db"],
                  b0, a1, a2, g, 30000.0, p["hiss"], p["hiss_hp"], p["lp_hz"],
                  SIZES[inst], p["rip_db"], p["rip_c"], p["slow_db"], p["slow_hz"], p["loud_db"],
                  p["vib_rate_dev"], p["vib_depth_dev"], p["vib_drift_hz"],
                  10 ** ((p["direct_db"] + BODY[inst].max()) / 20), p["ring"], p["lift"], p["ring_s"],
                  p["fl_db"], p["fl_hz"], p["fl_slope"], p["fl_hzslope"], p["jit_ct"], p["jit_hz"], p["pdrift_ct"], p["pdrift_hz"],
                  p["fl_knee"], p["fl_fslope"], p["rip_knee"], p["rip_fslope"], p["direct_hp"], p["fl_damp"], p["fl_clip"])
