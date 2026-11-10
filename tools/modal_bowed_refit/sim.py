"""Python twin of src/dsp/ModalBowed.h (bowed notes) for fitting (DEFAULT: the engine as
fitted, round 9). The release: lift=1 keeps the bow's speed as its pressure eases over rel_s
(lift=0, the bow slowing to a stop on the string, braked the note into a cliff about
0.35 s after the note-off), the upper partials' losses rise to ring times over ring_s
and fade_db fades the note (dB a second) once let go. Round 9: per-note tables (NOTES,
notes_<inst>.json), the violin's hall floor, the cello's release floor; see README.md.
Bit-identical to the fits' twins at size 1 and (to float rounding) at 2.1."""
import json
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
           fl_knee, fl_fslope, rip_knee, rip_fslope, direct_hp, fl_damp, fl_clip,
           direct_lp, rosin_hp, jit_hp,
           ob_db, ob_knee, ob_slope, ob_s,
           room_g, room_len, room_fb, room_p, rloss, rpow,
           ob_ksz, sw_sz,
           rdrop_db, rdrop_s, tail_db, tail_s, tail_t0, sw2, nz_amt, ntail, hum_k, hum_all, rgp,
           flr_g, flr_lp, flr_hp, flr_att, flr_rel, flr_rs, flr_fade, flr_w,
           pl_db, obadd, rladd, rdk, rdk_s, nfloor, nfloor_dbs):
    k = 1.0 / sr
    n_ = np.arange(1, modes + 1).astype(np.float64)
    shape = np.sin(n_ * np.pi * beta)
    push = 2.0 * k * shape / max(1e-6, np.sum(shape * shape))
    # (round 9: pl_db, the per-note tables' dB on each partial's share of the bridge force)
    out0 = n_ / modes * np.exp(DB * pl_db[:modes])
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
    # (round 7: and builds size ** sw_sz slower on a bigger body; the release as it was)
    sc_on = 1.0 - np.exp(-1.0 / (max(1e-3, swell_s * size ** sw_sz) * sr))
    # (round 8) The stroke builds as two such stages in a row (sw2 = 1: an S, the bow
    # gathering speed from rest; 0: the one stage as it was).
    swell_b = 0.0; jrat = 1.0
    hp_state = 0.0; hp_prev = 0.0
    hp_a = np.exp(-2 * np.pi * hiss_hp / sr)
    phase = 0.0; vib_env = 0.0
    lp_a = np.exp(-2 * np.pi * lp_hz / sr) if lp_hz < sr * 0.45 else 0.0
    lp = 0.0
    level = 1.0; fade = 10 ** (-fade_db / 20 / sr)
    # (round 8) The let-go in two stages, as Spitfire's (a release sample: a quick drop,
    # a short hold, a fall to a quiet tail of the hall): a gain after the room, at the
    # block rate, glided. It drops rdrop_db at once (over rdrop_s: the bow off the
    # string), then tail_db more as a smooth step from tail_t0 over tail_s; the room's
    # own ring goes on under it, that much quieter.
    rgain = 1.0; rgain_step = 0.0; nboost = 1.0
    # (round 9, the cello's) The hall's floor under the bow's noise: once let go, the hair's noise
    # falls no lower than nfloor times its level at the let-go, that floor fading nfloor_dbs dB a
    # second. (round 9, the cello's table) Each partial's own drop at the let-go, rdk dB over rdk_s.
    nfac_held = 0.0; nfl_now = 0.0; xdrop = 0.0
    # The box radiates little below its lowest resonance: the direct path is
    # high-passed (two poles, Butterworth) at direct_hp / size.
    use_hp = direct_hp > 0.0
    hb0 = 1.0; hb1 = 0.0; hb2 = 0.0; ha1 = 0.0; ha2 = 0.0
    if use_hp:
        # (round 8: and nothing of the string's is under its fundamental; what the twin has there is the
        # rosin's jitter leaning on the modes, a hum: the corner no lower than hum_k times the note)
        # (and, the whole bridge going through it, no higher than 0.7 times the note: a cello's low fundamentals stay)
        w0 = 2 * np.pi * min(max(min(direct_hp / size, 0.7 * f0) if hum_all > 0 else direct_hp / size, hum_k * f0), sr * 0.45) / sr
        al = np.sin(w0) / (2 * 0.7071067811865476)
        cw = np.cos(w0); a0 = 1 + al
        hb0 = (1 + cw) / 2 / a0; hb1 = -(1 + cw) / a0; hb2 = hb0; ha1 = -2 * cw / a0; ha2 = (1 - al) / a0
    hx1 = 0.0; hx2 = 0.0; hy1 = 0.0; hy2 = 0.0
    # (and falls away up high: low-passed, two poles, at direct_lp / size; a
    # big box's broad radiation is darker)
    use_lp = direct_lp > 0.0 and direct_lp / size < sr * 0.45
    lb0 = 1.0; lb1 = 0.0; lb2 = 0.0; la1 = 0.0; la2 = 0.0
    if use_lp:
        w0 = 2 * np.pi * (direct_lp / size) / sr
        al = np.sin(w0) / (2 * 0.7071067811865476)
        cw = np.cos(w0); a0 = 1 + al
        lb0 = (1 - cw) / 2 / a0; lb1 = (1 - cw) / a0; lb2 = lb0; la1 = -2 * cw / a0; la2 = (1 - al) / a0
    lx1 = 0.0; lx2 = 0.0; ly1 = 0.0; ly2 = 0.0
    # The bow's rosin noise has little in it low down: high-passed (two
    # poles) at rosin_hp / size before it reaches the bridge.
    use_rhp = rosin_hp > 0.0
    rb0 = 1.0; rb1 = 0.0; rb2 = 0.0; ra1 = 0.0; ra2 = 0.0
    if use_rhp:
        w0 = 2 * np.pi * min(rosin_hp / size, sr * 0.45) / sr
        al = np.sin(w0) / (2 * 0.7071067811865476)
        cw = np.cos(w0); a0 = 1 + al
        rb0 = (1 + cw) / 2 / a0; rb1 = -(1 + cw) / a0; rb2 = rb0; ra1 = -2 * cw / a0; ra2 = (1 - al) / a0
    rx1 = 0.0; rx2 = 0.0; ry1 = 0.0; ry2 = 0.0
    # The rosin's jitter on the bow's speed is a scrape, with little in it
    # low down (unfiltered, it hums under the note): high-passed, two poles,
    # at jit_hp / size.
    use_jhp = jit_hp > 0.0
    jb0 = 1.0; jb1 = 0.0; jb2 = 0.0; ja1 = 0.0; ja2 = 0.0
    if use_jhp:
        w0 = 2 * np.pi * min(jit_hp / size, sr * 0.45) / sr
        al = np.sin(w0) / (2 * 0.7071067811865476)
        cw = np.cos(w0); a0 = 1 + al
        jb0 = (1 + cw) / 2 / a0; jb1 = -(1 + cw) / a0; jb2 = jb0; ja1 = -2 * cw / a0; ja2 = (1 - al) / a0
    jx1 = 0.0; jx2 = 0.0; jy1 = 0.0; jy2 = 0.0
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
    # The onset's brightness (round 6): each partial's share of the bridge
    # force starts ob_db down, the full depth at ob_knee and over (as
    # (hz / ob_knee) ** ob_slope below it), and comes back over ob_s
    # (Spitfire's upper partials build up after the fundamental, where the
    # twin's came in first). Block rate, glided with the ripple's gains.
    # (Round 7: on a bigger body the darkening reaches lower, the knee at
    # ob_knee / size ** ob_ksz: a cello's upper partials come in later. None
    # on a violin.)
    obshape = ob_db * np.minimum(1.0, (hz0 / (ob_knee * size ** -ob_ksz)) ** ob_slope)
    obshape = obshape + obadd[:modes]   # (round 9: the cello's table's onset darkening by partial, dB)
    ob_now = obshape.copy()
    out = out0 * np.exp(-DB * ob_now)
    # The room (round 6): four delay lines (room_len samples) mixed by a
    # Householder matrix, each through a one-pole low-pass (room_fb its gain
    # at DC, room_p its pole) so the low end rings longer than the top; it
    # hears the whole note and keeps ringing after it (room_g its return).
    nr = len(room_len)
    rbuf = np.zeros((nr, room_len.max() if nr else 1))
    ridx = np.zeros(nr, np.int64)
    rlp = np.zeros(nr)
    rsign = np.array([1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0])
    use_room = room_g > 0.0 and nr > 0
    # (round 9, the violin's) The hall's floor: a pink noise (equal energy per octave), one-pole
    # low-passed at flr_lp and high-passed at flr_hp, flr_g against the note, after the let-go's
    # gain (it does not drop with the note). It rises over flr_att from the note-on; once let go it
    # steps to flr_rel times (glided over flr_rs) and fades flr_fade dB a second. One noise and
    # five one-poles a sample.
    use_flr = flr_g > 0.0
    fs_ = s ^ 0x27D4EB2F
    pk0 = 0.0; pk1 = 0.0; pk2 = 0.0
    fla = np.exp(-2 * np.pi * min(flr_lp, sr * 0.45) / sr); flp = 0.0
    fha = np.exp(-2 * np.pi * flr_hp / sr); fhp = 0.0; fhx = 0.0
    fe = 0.0; fe_step = 0.0
    fatt = 1.0 - np.exp(-32.0 / (max(1e-3, flr_att) * sr))
    rladd = rladd[:modes]; rdk = rdk[:modes]
    use_rl = np.any(rladd != 0.0)
    for i in range(total):
        if i % 32 == 0 and i >= held:
            # Let go: the upper partials' losses rise to ring times over
            # ring_s (the lifting bow's hair stops feeding them).
            gone = min(1.0, (i - held) / (max(1e-3, ring_s) * sr))
            # (round 8: the released partials' extra loss comes in as gone ** rgp, late
            # on a big body, rgp = size ** rg_k; 1 on a violin)
            gr = gone if rgp == 1.0 else gone ** rgp
            # (round 9: + the cello's table's released loss by partial, no less than a tenth of the global's)
            if use_rl:
                rl0 = rloss * (hz0 / 1000.0) ** rpow
                sigma = loss0 + loss1 * (1.0 + (ring - 1.0) * gone) * hz0 * hz0 + gr * np.maximum(rl0 + rladd, 0.1 * rl0)
            else:
                sigma = loss0 + loss1 * (1.0 + (ring - 1.0) * gone) * hz0 * hz0 + gr * rloss * (hz0 / 1000.0) ** rpow
            decay = np.exp(-sigma * k)
            fall = decay * decay
            tr = (i + 32 - held) / sr
            xs = min(1.0, max(0.0, (tr - tail_t0) / max(1e-3, tail_s)))
            rtarget = np.exp(-DB * (rdrop_db * (1.0 - np.exp(-tr / max(1e-4, rdrop_s))) + tail_db * xs * xs * (3.0 - 2.0 * xs)))
            rgain_step = (rtarget - rgain) / 32.0
            # (the hair's noise is not in that fall: the lifted bow's breath and the hall's
            # hiss carry on under the tail, fading with the bow's pressure; ntail = 1 keeps
            # all of it, 0 lets it fall with the note)
            nboost = rtarget ** -ntail
            nfl_now = nfloor * nfac_held * np.exp(-DB * nfloor_dbs * tr)
            xdrop = 1.0 - np.exp(-tr / rdk_s)
            if use_flr:
                xr = min(1.0, tr / max(1e-3, flr_rs))
                ftarget = (1.0 + (flr_rel - 1.0) * xr) * 10 ** (-flr_fade * tr / 20.0)
                fe_step = (ftarget - fe) / 32.0
        elif i % 32 == 0 and use_flr:
            fe_step = ((fe + fatt * (1.0 - fe)) - fe) / 32.0
        if i % 32 == 0:
            t = i / sr
            ob_now = obshape * np.exp(-t / ob_s)
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
            out_step = (out0 * np.exp(DB * (ripple + wander + (0.0 if fl_damp > 0 else 1.0) * flut * fshape - ob_now - rdk * xdrop)) - out) / 32.0
        nxt = turn * q - fall * qp
        v = np.sum(shape * (nxt - qp)) * 0.5 * sr
        on = 1.0 if i < held else 0.0
        env += (on - env) * (ac if on > 0 else rc)
        swell += (on - swell) * (sc_on if on > 0 else sc)
        swell_b += (swell - swell_b) * (sc_on if on > 0 else sc)
        # The bow's speed: lift > 0 keeps it going as the bow leaves the
        # string (a lift, not a stop that brakes the string).
        fac = env * (1.0 - swell_amt + swell_amt * (swell + sw2 * (swell_b - swell)))
        # (round 8) The hair's noise is there from the start (Spitfire's floor between the
        # partials is up within 50 ms, the partials take half a second): its own, shallower
        # swell, nz_amt deep (under 0: the stroke's, as it was).
        nfac = fac if nz_amt < 0 else env * (1.0 - nz_amt + nz_amt * (swell + sw2 * (swell_b - swell)))
        if on > 0:
            nfac_held = nfac
        elif nfac < nfl_now:
            nfac = nfl_now
        if on > 0:
            senv = fac
        else:
            senv = fac + lift * (senv - fac)
        s = (s * 1664525 + 1013904223) % 4294967296
        w1 = (s >> 8) * (2.0 / 16777216.0) - 1.0
        if use_jhp:
            wf = jb0 * w1 + jb1 * jx1 + jb2 * jx2 - ja1 * jy1 - ja2 * jy2
            jx2 = jx1; jx1 = w1; jy2 = jy1; jy1 = wf
            w1 = wf
        s = (s * 1664525 + 1013904223) % 4294967296
        w2 = (s >> 8) * (2.0 / 16777216.0) - 1.0
        force = 0.0
        noise_out = 0.0
        if env > 1e-5:
            speed = vbow * loud_f * senv
            bow = speed * (1.0 + 0.6 * vel * bite) * (1.0 + w1 * jitter_amt * jrat)
            dv = bow - v
            slope = 5.0 - 4.0 * grip
            table = min(1.0, (abs(dv * slope) + 0.75) ** -4.0)
            force = dv * table * env
            noise_out = w2 * vbow * loud_f * nfac
            if on > 0 and i % 32 == 0:
                jrat = nfac / max(1e-6, fac)   # (the rosin's jitter too: as strong against the full stroke from the start; held at its last on the let-go)
        bite *= bite_fall
        nxt = nxt + push * force
        qp = q; q = nxt
        out = out + out_step
        if i >= held:
            level *= fade
        rn = rosin * noise_out * 1e-4 * nboost
        if use_rhp:
            rf = rb0 * rn + rb1 * rx1 + rb2 * rx2 - ra1 * ry1 - ra2 * ry2
            rx2 = rx1; rx1 = rn; ry2 = ry1; ry1 = rf
            rn = rf
        bridge = (np.sum(out * q) + rn) * level
        dr = bridge
        if use_hp and hum_all > 0:
            # (hum_all: the whole bridge through it, the body's resonators too, not only the direct path)
            dr = hb0 * bridge + hb1 * hx1 + hb2 * hx2 - ha1 * hy1 - ha2 * hy2
            hx2 = hx1; hx1 = bridge; hy2 = hy1; hy1 = dr
            bridge = dr
        elif use_hp:
            dr = hb0 * bridge + hb1 * hx1 + hb2 * hx2 - ha1 * hy1 - ha2 * hy2
            hx2 = hx1; hx1 = bridge; hy2 = hy1; hy1 = dr
        if use_lp:
            dl = lb0 * dr + lb1 * lx1 + lb2 * lx2 - la1 * ly1 - la2 * ly2
            lx2 = lx1; lx1 = dr; ly2 = ly1; ly1 = dl
            dr = dl
        snd = direct * dr
        for j in range(nb):
            yy = bz_b0[j] * (bridge - bx2[j]) - bz_a1[j] * by1[j] - bz_a2[j] * by2[j]
            bx2[j] = bx1[j]; bx1[j] = bridge; by2[j] = by1[j]; by1[j] = yy
            snd += bz_g[j] * yy
        # hiss: high-passed noise straight to the output (bow hair on the string)
        hp = hp_a * (hp_state + noise_out - hp_prev)
        hp_prev = noise_out; hp_state = hp
        raw = snd * outgain + hiss * hp
        if use_room:
            tot = 0.0
            for r in range(nr):
                rlp[r] = room_fb[r] * (1.0 - room_p[r]) * rbuf[r, ridx[r]] + room_p[r] * rlp[r]
                tot += rlp[r]
            tot *= 2.0 / nr
            wet = 0.0
            for r in range(nr):
                wet += rsign[r] * rlp[r]
                rbuf[r, ridx[r]] = rlp[r] - tot + rsign[r] * raw
                ridx[r] += 1
                if ridx[r] >= room_len[r]:
                    ridx[r] = 0
            raw = raw + room_g * wet
        rgain += rgain_step
        raw *= rgain
        if use_flr:
            fs_ = (fs_ * 1664525 + 1013904223) % 4294967296
            wn = (fs_ >> 8) * (2.0 / 16777216.0) - 1.0
            pk0 = 0.99765 * pk0 + wn * 0.0990460
            pk1 = 0.96300 * pk1 + wn * 0.2965164
            pk2 = 0.57000 * pk2 + wn * 1.0526913
            pn = pk0 + pk1 + pk2 + wn * 0.1848
            flp = pn + fla * (flp - pn)
            fhp = fha * (fhp + flp - fhx); fhx = flp
            fe += fe_step
            raw += flr_g * fe * (fhp + flr_w * wn)
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


DEFAULT = dict(beta=0.11, loss0=0.8, loss1=3.244e-06, bright=1.7841, b=1.0e-5, grip=0.5, speed=0.5, vel=80 / 127,
               vib_ct=14.65, vib_rate=5.2, vib_delay=0.25, vib_rise=0.4, swell_s=0.20311, swell_amt=0.91629,
               rosin=0.017985, jitter=0.08536, rel_s=0.30149, fade_db=22.334, hiss=0.00017936, hiss_hp=9836.2, top=12000.0,
               attack_scale=0.050102, lp_hz=5701.8,
               rip_db=3.326, rip_c=19.46, slow_db=2.052, slow_hz=0.6, loud_db=1.742,
               vib_rate_dev=0.09024, vib_depth_dev=0.4561, vib_drift_hz=0.8, direct_db=-12.074, ring=9.3783, lift=1.0, ring_s=0.4991,
               fl_db=21.35, fl_hz=16.51, fl_slope=0.0, fl_hzslope=0.7853, jit_ct=0.0, jit_hz=20.0, pdrift_ct=1.441, pdrift_hz=0.5,
               fl_knee=818.2, fl_fslope=1.285, rip_knee=963.9, rip_fslope=2.074, direct_hp=57.747, fl_damp=1.0, fl_clip=30.0,
               direct_lp=20000.0, rosin_hp=0.0, jit_hp=0.0,
               ob_db=17.49, ob_knee=2689.6, ob_slope=2.9034, ob_s=0.15823,
               ob_ksz=1.56, sw_sz=0.25,
               room_db=-2.5, room_sz=-4.1276, room_ms=18.44, room_lo=38.88, room_hi=159.5, rloss=1.2938, rpow=3.2245,
               room_lk=1.086, room_hk=0.19131, rloss_k=11.925,
               ring_k=-2.297, rpow_k=1.053, rg_k=1.957, fade_k=0.2405,
               dir_sz=-7.6977, dhp_k=0.87, vib_k=-0.098769, obs_k=0.96,
               rdrop_db=3.092, rdrop_s=0.02, tail_db=22.875, tail_s=0.50944, tail_t0=0.07799, sw2=0.67352, nz_amt=0.26398, ntail=0.34346,
               hum_k=0.37266, hum_all=1.0,
               flr_db=-55.793, flr_k=4.9665, flr_lp=1874.2, flr_hp=226.92, flr_att=0.0035533, flr_rel_db=2.2157, flr_rs=0.05,
               flr_fade=23.941, flr_w=0.30072, hiss_k=0.77496,
               nfloor_db=-11.7, nfloor_dbs=20.0, rdk_s=0.03)

# (Round 8) The two-stage let-go (rel_s, fade_db, ring, rloss, rdrop_db, tail_db, ntail) was fitted to the
# violin's partial trajectories. On held-out notes (2 seeds) it brings the trajectory error after
# the note-off from 9.5 to 6.3 dB and look.py's release term, counted over the cells within 40 dB
# of the note's peak, from 9.6 to 7.4; the unmasked term goes from 7.8 to 9.25 (over half of it
# is cells more than 40 dB down, where Spitfire's tail is a low, dark noise). RELEASE7 is the
# release as it was (play(inst, note, **RELEASE7)).
RELEASE7 = dict(rel_s=1.0, fade_db=35.79, ring=17.84, rloss=23.94, rdrop_db=0.0, tail_db=0.0, ntail=0.0)

# The violin's terms (DEFAULT) blend to these at the cello's size, by t = log(size) / log(2.1),
# held to 0..1 (play()); the cello's own terms are the size powers in play(), on top of these.
# Round 8: the cello keeps round 7's onset and let-go (the new ones off). Round 9: the violin's
# retuned globals blend back to their round 8 values at the cello's size (the cello was fitted
# without them), and the cello's own round 9 values (flutter, wander, ripple, hiss) blend in
# from the violin's. The violin's hall floor (flr_*) fades out by (1 - t) in amplitude, the
# cello's (nfloor_db) in by t. hum_all is not blended.
CELLO = dict(swell_s=0.3544, swell_amt=0.8208, ob_db=15.04, ob_knee=1990.0, ob_slope=2.82, ob_s=0.1429,
             sw2=0.0, nz_amt=0.8208, **RELEASE7,
             attack_scale=0.05, bright=2.144, loss1=3.947e-06, direct_db=-10.33, direct_hp=96.61, lp_hz=5590.0,
             rosin=0.01695, hum_k=0.33, hiss_hp=8000.0, hiss_k=0.0,
             hiss=0.0022, fl_db=14.316, slow_db=1.5021, loud_db=0.87783, rip_db=2.9949)

# (Round 9) The per-note tables, each fitted at one size (notes_<inst>.json; write_engine.py puts
# them in ModalBowedNotes.h). Per note: gain_db (dB on each partial's share of the bridge force),
# dark_db (dB more onset darkening), loss_add (1/s more released loss), drop_db (each partial's own
# drop at the let-go), and scalars that replace the value at that size for that note (before the
# size powers). Read at note-on by the nearest MIDI note, never between notes (the violin fit found
# that worse than no table). A table is all there at its own size and gone a quarter of the way
# (in log size) toward the other: weight 1 - 4 |u - u_table|, u = log(size) / log(2.1).
_HERE = os.path.dirname(os.path.abspath(__file__))
NOTES = [json.load(open(os.path.join(_HERE, "notes_%s.json" % inst))) for inst in ("violin", "cello")]
TABLE_SCALARS = ("flr_db", "tail_db", "rdrop_db", "fade_db", "ob_db", "swell_s", "vib_ct", "slow_db", "rip_db", "rel_s", "ring")
TABLE_ARRAYS = ("gain_db", "dark_db", "loss_add", "drop_db")


def note_rows(size, note, tables=None):
    """[(weight, row)] for a note at a body size: the tables that reach this size and note."""
    u = np.log(size) / np.log(2.1)
    rows = []
    for tab in (NOTES if tables is None else tables):
        w = min(1.0, max(0.0, 1.0 - 4.0 * abs(u - np.log(tab["size"]) / np.log(2.1))))
        n = int(np.floor(note + 0.5))
        if n < tab["first"]:
            if not tab["hold_below"]:
                continue
            n = tab["first"]
        if n > tab["last"]:
            w *= max(0.0, 1.0 - (n - tab["last"]) / tab["fade_above"]) if tab["fade_above"] > 0 else 0.0
            n = tab["last"]
        if w > 0.0:
            rows.append((w, tab["notes"][str(n)]))
    return rows


def _arr(rows, key):
    a = np.zeros(64)
    for w, row in rows:
        if key in row:
            v = np.asarray(row[key], float)[:64]
            a[:len(v)] += w * v
    return a


ROOM_RATIOS = np.array([0.8013, 0.9137, 1.0798, 1.2052])  # (the lines' lengths against room_ms, near-even, coprime once rounded)


def _prime(n):
    n = max(3, int(n)) | 1
    while any(n % k == 0 for k in range(3, int(n ** 0.5) + 1, 2)):
        n += 2
    return n


def room_coeffs(p, sr=SR):
    """The room's four lines: lengths (room_ms mean, prime), each line's gain
    at DC for a decay of room_lo dB a second, and its one-pole's pole for
    room_hi dB a second at 4 kHz; the return for a wet level of room_db
    (energy, against the dry note, for a broad noise at DC)."""
    if p.get("room_db", -200.0) <= -100.0:
        return 0.0, np.zeros(0, np.int64), np.zeros(0), np.zeros(0)
    lens = np.array([_prime(round(p["room_ms"] * 1e-3 * sr * r)) for r in ROOM_RATIOS], np.int64)
    fb = 10 ** (-p["room_lo"] * lens / sr / 20.0)
    extra = np.maximum(0.0, p["room_hi"] - p["room_lo"]) * lens / sr  # dB more per pass at 4 kHz
    c = (10 ** (extra / 10.0) - 1.0) / (2.0 * (1.0 - np.cos(2 * np.pi * 4000.0 / sr)))
    pole = np.where(c > 1e-12, ((2 * c + 1) - np.sqrt(4 * c + 1)) / (2 * np.maximum(c, 1e-12)), 0.0)
    gm = np.mean(fb ** 2)
    g = 10 ** (p["room_db"] / 20.0) * np.sqrt((1.0 - gm) / len(lens))
    return float(g), lens, fb, pole


def play(inst, note, seconds=4.8, held=4.0, seed=1, tables=None, **kw):
    """tables: None plays the per-note tables (NOTES), [] none (a fit of a table passes [] and its
    own values in kw); the tables' scalars replace kw's too."""
    p = dict(DEFAULT); p.update(kw)
    t = min(1.0, max(0.0, np.log(SIZES[inst]) / np.log(2.1)))
    if t > 0.0:
        cel = dict(CELLO); cel.update({k: v for k, v in kw.items() if k in CELLO})   # (a value given by hand holds at every size)
        for key, v in cel.items():
            p[key] = v if t >= 1.0 else p[key] + (v - p[key]) * t
    # (round 9) The per-note tables: their scalars replace the value at this size, by the table's weight.
    rows = note_rows(SIZES[inst], note, tables)
    for w, row in rows:
        for key in TABLE_SCALARS:
            if key in row:
                p[key] = row[key] if w >= 1.0 else p[key] + (row[key] - p[key]) * w
    f0 = 440 * 2 ** ((note - 69) / 12)
    modes = int(max(1, min(64, min(p["top"], SR * 0.45) / f0)))
    attack = 0.05 * min(2.5, max(1.0, 220.0 / f0)) * p["attack_scale"]
    # The register's brightness: the upper strings lose less up high.
    loss1 = p["loss1"] * (f0 / 440.0) ** -p["bright"]
    b0, a1, a2, g = body_coeffs(inst)
    # (room_sz: the room's level in dB per doubling of the body's size against a
    # violin; round 7: a bigger body's room also rings longer and darker, its
    # decays room_lo and room_hi times size ^ room_lk and size ^ room_hk, and
    # its released partials lose more up high, rloss times size ^ rloss_k: all
    # 1 at a violin's size)
    sz = SIZES[inst]
    # (round 8, cello only, all exactly 1 (or + 0) at a violin's size. The release by
    # partial: the low partials ring on (ring's excess times size ^ ring_k) while the
    # upper ones die steeply and late (rpow times size ^ rpow_k, the ramp's power
    # size ^ rg_k, rloss_k retuned) and the fade is size ^ fade_k quicker. The wood:
    # the direct path dir_sz dB down per doubling of size (the body's resonances carve
    # the spectrum; the cello's gains refitted with it), its high-pass at
    # direct_hp / size ^ (1 - dhp_k), the vibrato size ^ vib_k as deep. The onset: the
    # upper partials' darkening lasts size ^ obs_k longer (ob_ksz, sw_sz retuned).)
    p["ring"] = 1.0 + (p["ring"] - 1.0) * sz ** p["ring_k"]
    p["rpow"] = p["rpow"] * sz ** p["rpow_k"]
    p["fade_db"] = p["fade_db"] * sz ** p["fade_k"]
    p["direct_hp"] = p["direct_hp"] * sz ** p["dhp_k"]
    p["direct_db"] = p["direct_db"] + p["dir_sz"] * np.log2(sz)
    p["vib_ct"] = p["vib_ct"] * sz ** p["vib_k"]
    p["ob_s"] = p["ob_s"] * sz ** p["obs_k"]
    rg, rl, rfb, rp = room_coeffs(dict(p, room_db=p["room_db"] + p["room_sz"] * np.log2(sz),
                                       room_lo=p["room_lo"] * sz ** p["room_lk"], room_hi=p["room_hi"] * sz ** p["room_hk"]))
    return kernel(f0, int(seconds * SR), int(held * SR), SR, modes, p["beta"], p["loss0"], loss1, p["b"], p["grip"],
                  0.05 + 0.3 * p["speed"], p["vel"], seed, p["vib_ct"], p["vib_rate"], p["vib_delay"], p["vib_rise"],
                  attack, p["swell_s"], p["swell_amt"], p["rosin"], p["jitter"], p["rel_s"], p["fade_db"],
                  b0, a1, a2, g, 30000.0, p["hiss"] * (f0 / 440.0) ** p["hiss_k"], p["hiss_hp"], p["lp_hz"],
                  SIZES[inst], p["rip_db"], p["rip_c"], p["slow_db"], p["slow_hz"], p["loud_db"],
                  p["vib_rate_dev"], p["vib_depth_dev"], p["vib_drift_hz"],
                  10 ** ((p["direct_db"] + BODY[inst].max()) / 20), p["ring"], p["lift"], p["ring_s"],
                  p["fl_db"], p["fl_hz"], p["fl_slope"], p["fl_hzslope"], p["jit_ct"], p["jit_hz"], p["pdrift_ct"], p["pdrift_hz"],
                  p["fl_knee"], p["fl_fslope"], p["rip_knee"], p["rip_fslope"], p["direct_hp"], p["fl_damp"], p["fl_clip"],
                  p["direct_lp"], p["rosin_hp"], p["jit_hp"],
                  p["ob_db"], p["ob_knee"], p["ob_slope"], max(1e-3, p["ob_s"]),
                  rg, rl, rfb, rp, p["rloss"] * sz ** p["rloss_k"], p["rpow"],
                  p["ob_ksz"], p["sw_sz"],
                  p["rdrop_db"], p["rdrop_s"], p["tail_db"], p["tail_s"], p["tail_t0"], p["sw2"], p["nz_amt"], p["ntail"], p["hum_k"], p["hum_all"], sz ** p["rg_k"],
                  (1.0 - t) * 10 ** (p["flr_db"] / 20) * (f0 / 440.0) ** (p["flr_k"] / 6.0206) if t < 1.0 and p["flr_db"] > -150 else 0.0,
                  p["flr_lp"], p["flr_hp"], p["flr_att"], 10 ** (p["flr_rel_db"] / 20), p["flr_rs"], p["flr_fade"], p["flr_w"],
                  _arr(rows, "gain_db"), _arr(rows, "dark_db"), _arr(rows, "loss_add"), _arr(rows, "drop_db"), p["rdk_s"],
                  t * 10 ** (p["nfloor_db"] / 20) if p["nfloor_db"] > -100 else 0.0, p["nfloor_dbs"])
