"""Python twin of src/dsp/ModalBowed.h for fitting (DEFAULT: the engine as fitted;
rosin and fade_db are tried-and-dropped extras, 0 in the engine)."""
import re
import numpy as np
from numba import njit

SR = 48000
H = open(__import__("os").path.join(__import__("os").path.dirname(__file__), "..", "..", "src", "dsp", "ModalBowedBodies.h")).read()


def arr(name):
    m = re.search(r"float %s\[count\] \{([^}]*)\}" % name, H)
    return np.array([float(v.strip().rstrip("f")) for v in m.group(1).split(",")])


BODY = {"hz": arr("hz"), "bw": arr("bandwidth"), "violin": arr("violin"), "viola": arr("violas"), "cello": arr("cellos")}
SIZES = {"violin": 1.0, "viola": 1.17, "cello": 2.1}


@njit(cache=True)
def kernel(f0, total, held, sr, modes, beta, loss0, loss1, b, grip, vbow, vel, seed,
           vib_ct, vib_rate, vib_delay, vib_rise, attack_s, swell_s, swell_amt,
           rosin, jitter_amt, rel_s, fade_db,
           bz_b0, bz_a1, bz_a2, bz_g, outgain, hiss, hiss_hp):
    k = 1.0 / sr
    n_ = np.arange(1, modes + 1).astype(np.float64)
    shape = np.sin(n_ * np.pi * beta)
    push = 2.0 * k * shape / max(1e-6, np.sum(shape * shape))
    out = n_ / modes
    q = np.zeros(modes); qp = np.zeros(modes)
    turn = np.zeros(modes); fall = np.zeros(modes)
    nb = len(bz_b0)
    bx1 = np.zeros(nb); bx2 = np.zeros(nb); by1 = np.zeros(nb); by2 = np.zeros(nb)
    y = np.zeros(total)
    ac = 1.0 - np.exp(-1.0 / (attack_s * sr))
    rc = 1.0 - np.exp(-1.0 / (rel_s * sr))
    bite_fall = np.exp(-1.0 / (0.08 * sr))
    env = 0.0; bite = 1.0; s = seed; swell = 0.0
    sc = 1.0 - np.exp(-1.0 / (max(1e-3, swell_s) * sr))
    hp_state = 0.0; hp_prev = 0.0
    hp_a = np.exp(-2 * np.pi * hiss_hp / sr)
    phase = 0.0; vib_env = 0.0
    level = 1.0; fade = 10 ** (-fade_db / 20 / sr)
    for i in range(total):
        if i % 32 == 0:
            t = i / sr
            if t > vib_delay:
                vib_env = min(1.0, (t - vib_delay) / max(1e-3, vib_rise))
            cents = vib_ct * vib_env * np.sin(phase)
            f = f0 * 2.0 ** (cents / 1200.0)
            hz = np.minimum(n_ * f * np.sqrt(1.0 + b * n_ * n_), sr * 0.49)
            sigma = loss0 + loss1 * hz * hz
            decay = np.exp(-sigma * k)
            turn = 2.0 * decay * np.cos(2 * np.pi * hz * k)
            fall = decay * decay
        phase += 2 * np.pi * vib_rate / sr
        nxt = turn * q - fall * qp
        v = np.sum(shape * (nxt - qp)) * 0.5 * sr
        on = 1.0 if i < held else 0.0
        env += (on - env) * (ac if on > 0 else rc)
        swell += (on - swell) * sc
        s = (s * 1664525 + 1013904223) % 4294967296
        w1 = (s >> 8) * (2.0 / 16777216.0) - 1.0
        s = (s * 1664525 + 1013904223) % 4294967296
        w2 = (s >> 8) * (2.0 / 16777216.0) - 1.0
        force = 0.0
        noise_out = 0.0
        if env > 1e-5:
            bow = vbow * env * (1.0 - swell_amt + swell_amt * swell) * (1.0 + 0.6 * vel * bite) * (1.0 + w1 * jitter_amt)
            d = bow - v
            slope = 5.0 - 4.0 * grip
            table = min(1.0, (abs(d * slope) + 0.75) ** -4.0)
            force = d * table * env
            noise_out = w2 * vbow * env * (1.0 - swell_amt + swell_amt * swell)
        bite *= bite_fall
        nxt = nxt + push * force
        qp = q; q = nxt
        if i >= held:
            level *= fade
        bridge = (np.sum(out * q) + rosin * noise_out * 1e-4) * level
        snd = 0.0
        for j in range(nb):
            yy = bz_b0[j] * (bridge - bx2[j]) - bz_a1[j] * by1[j] - bz_a2[j] * by2[j]
            bx2[j] = bx1[j]; bx1[j] = bridge; by2[j] = by1[j]; by1[j] = yy
            snd += bz_g[j] * yy
        # hiss: high-passed noise straight to the output (bow hair on the string)
        hp = hp_a * (hp_state + noise_out - hp_prev)
        hp_prev = noise_out; hp_state = hp
        y[i] = snd * outgain + hiss * hp
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


DEFAULT = dict(beta=0.11, loss0=0.8, loss1=3.0e-5, b=1.0e-5, grip=0.5, speed=0.5, vel=80 / 127,
               vib_ct=9.0, vib_rate=5.2, vib_delay=0.25, vib_rise=0.4, swell_s=0.34, swell_amt=0.97,
               rosin=0.0, jitter=0.0905, rel_s=0.002, fade_db=0.0, hiss=0.0052, hiss_hp=8000.0, top=9000.0, attack_scale=0.05)


def play(inst, note, seconds=4.8, held=4.0, seed=1, **kw):
    p = dict(DEFAULT); p.update(kw)
    f0 = 440 * 2 ** ((note - 69) / 12)
    modes = int(max(1, min(64, min(p["top"], SR * 0.45) / f0)))
    attack = 0.05 * min(2.5, max(1.0, 220.0 / f0)) * p["attack_scale"]
    b0, a1, a2, g = body_coeffs(inst)
    return kernel(f0, int(seconds * SR), int(held * SR), SR, modes, p["beta"], p["loss0"], p["loss1"], p["b"], p["grip"],
                  0.05 + 0.3 * p["speed"], p["vel"], seed, p["vib_ct"], p["vib_rate"], p["vib_delay"], p["vib_rise"],
                  attack, p["swell_s"], p["swell_amt"], p["rosin"], p["jitter"], p["rel_s"], p["fade_db"],
                  b0, a1, a2, g, 30000.0, p["hiss"], p["hiss_hp"])
