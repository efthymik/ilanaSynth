"""Python twin of src/dsp/ModalBowed.h (bowed notes) for fitting (DEFAULT: the engine as
fitted, round 11). The release: lift=1 keeps the bow's speed as its pressure eases over rel_s
(lift=0, the bow slowing to a stop on the string, braked the note into a cliff about
0.35 s after the note-off), the upper partials' losses rise to ring times over ring_s
and fade_db fades the note (dB a second) once let go. Round 9: per-note tables (NOTES,
notes_<inst>.json), the violin's hall floor, the cello's release floor; see README.md. Round 10 (the violin's,
all faded out toward the cello): the body's air mode rung by the hair's noise (a0_*) and the 300-500 Hz modes
(b1_*), the hall floor's second high-pass and rumble (flr_ord, rb_*), a louder hiss rising with the note up to
hiss_cap and building over hs_s at the onset with the rosin's noise (hs_*), a faster wander (slow_hz), and the
violin table's onset darkening by partial (dark_db) and per-note hiss_db, a0_db. Round 10, the cello's (all by t):
a sine wander per partial (sn_*), the flutter's high-pass (fl_hpz), the low partials' level vibrato (am_*),
the low floor on the hall floor's path (cflr_*, per note cflr_add), the string's top past its last mode
(hf_*, per note hf_add), the release floor and size powers retuned, CELLO's vibrato and flutter terms.
vib_ct is the plugin's VIBRATO knob at its default 0.5; the knob v plays 30 v ^ log2(30 / vib_ct)
cents (round 10; it was 30 v ^ 1.737, 9 cents at 0.5, and v = 0.662 gave the fitted 14.65).
Round 11, the cello's (all by t): the low partials ring on after the let-go (lr_*), and the table's gain envelope by
octave and time knot (kn_on over the first 0.9 s, kn_rl after the let-go), fitted with the bow frozen.
Round 11, the violin's: the residual layers (rs*_, per-note rs0_db / rs1_db / rs2_db in the violin's table): the
note's own noise between and under the partials as shaped noise loops (sustain, onset, release) synthesised from
the table's band levels, in place of the hall floor, rumble, air mode and 300-500 Hz modes' noise (rs_replace)
wherever the violin's table reaches. Each loop jumps to a random place once a length (a 15 ms
equal-power crossfade), so no layer repeats with its loop's period.
Round 12, the violin's (all in the violin's table, by its weight; nothing reaches the cello): the held note's level over
time (kv_su at su_t), the partials' onset and release envelope by octave and knot (kv_on, kv_rl: the cello's kn_* mechanism),
and each residual layer's onset and release envelope by knot (rk_on, rk_rl), fitted with the bow frozen; the table's slow_db at 0.35 of round 11's.
Round 12, the cello's (by its table and t; nothing at a violin's size): its own residual layers (the cello table's rs*_db,
note_rows(layer=True): w (1 - |t - u_table|)) with their own envelope (rs_env), replacing its low floor, string top and release
floor (rsc_*); per-note sn_db and am_db; kn_on / kn_rl refitted; CELLO's lp_hz 3000.
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
           pl_db, obadd, rladd, rdk, rdk_s, nfloor, nfloor_dbs,
           rnb, a0_g, a0_b0, a0_a1, a0_a2, flr_ord, a0_fl, rb_g, rb_b0, rb_a1, rb_a2, b1_in, hs_depth, hs_s, hs_rn,
           hf_g, hf_hz, hf_src, hf_lp,
           sn_amp, sn_hz, sn_spread, am_amp, am_ph, am_dly, fl_hpa, flr_o2,
           lr_w, lr_cap, kon, kon_t, krl, krl_t, rs_t1, rs_t2, rs_p,
           rs_kon, rs_kon_t, rs_krl, rs_krl_t, ksu, ksu_t, rs_ksu, mmask, ymask, rsmask):
    k = 1.0 / sr
    # (round 11, the violin's) The residual layers: per-note shaped noise loops (rs_t1, rs_t2: two loops
    # of coprime lengths per layer, each layer's spectrum the note's own), added at the output. rs_p[l]:
    # (on, att_s, tau_s, rel_db, rel_s, fade_db, start). Layer envelopes (amplitude, block rate, glided):
    # held: (1 - exp(-t / att)) * (exp(-t / tau) if tau > 0 else 1); let go: the held value then
    # rel_db down over rel_s and fade_db dB a second (start = 1: a release layer, 0 while held, rising
    # over rel_s after the let-go). One table read per layer a sample.
    nrs = rs_t1.shape[0]
    rs_g = np.zeros(nrs); rs_step = np.zeros(nrs); rs_held = np.zeros(nrs)
    rs_i1 = (seed * 7919) % rs_t1.shape[1]; rs_i2 = (seed * 104729) % rs_t2.shape[1]
    # Each loop jumps to a random place once a length (so it doesn't repeat with its own period),
    # crossfading over rs_fn samples (0.015 s), equal power (sin / cos of a quarter turn): the two
    # places are uncorrelated noise. (rs_j*: where it fades to, rs_f*: how far, -1 none; rs_u*: until
    # the next jump.)
    rs_fn = max(1, int(round(0.015 * sr)))
    rs_fin = np.sin(0.5 * np.pi * (np.arange(rs_fn) + 0.5) / rs_fn)
    rs_j1 = 0; rs_j2 = 0; rs_f1 = -1; rs_f2 = -1
    rs_u1 = rs_t1.shape[1] - rs_fn; rs_u2 = rs_t2.shape[1] - rs_fn
    rs_rng = (seed * 2654435761 + 1779033703) % 4294967296
    use_rkon = rs_kon.shape[1] > 0
    use_rksu = rs_ksu.shape[1] > 0
    use_rkrl = rs_krl.shape[1] > 0
    # (fitting only, not in the engine: mmask scales each partial's share of the bridge, ymask everything but
    # the layers, rsmask each layer: the frozen-bow decomposition)
    use_mm = len(mmask) == modes
    use_ym = len(ymask) == 1
    use_rsm = len(rsmask) == rs_t1.shape[0]
    rs_on = False
    for l in range(nrs):
        if rs_p[l, 0] > 0:
            rs_on = True
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
    # (round 10, the cello's) the flutter without its slow part: less a one-pole of itself (pole fl_hpa a block)
    flsl = np.zeros(modes); flhp = np.zeros(modes)
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
    ax1 = 0.0; ax2 = 0.0; ay1 = 0.0; ay2 = 0.0
    hs_env = 1.0 - hs_depth
    fhp2 = 0.0; fhx2 = 0.0; pnl = 0.0; rbx1 = 0.0; rbx2 = 0.0; rby1 = 0.0; rby2 = 0.0
    use_rl = np.any(rladd != 0.0)
    # (round 10, the cello's) The string's top above its last mode: the bow's friction force (all
    # its harmonics) high-passed (four poles) at hf_hz, hf_g into the radiated sound.
    # (round 10, the cello's) The slow wander as a sine per partial (sn_amp dB by partial, sn_hz
    # spread over sn_hz * (1 +- sn_spread), its phase from the seed): it moves each partial as
    # much in every take and sums to nothing over a second or two, where the noise wander left
    # a random tilt on the partials' levels. A phasor turned once a block, per partial.
    # And the low partials' level vibrato (am_amp dB by partial, am_ph its lead on the pitch's
    # vibrato), coming in am_dly after the pitch's.
    use_sn = np.any(sn_amp != 0.0)
    use_am = np.any(am_amp != 0.0)
    sq = np.uint32(s ^ 0x165667B1)
    snc = np.zeros(modes); sns = np.zeros(modes); snw = np.zeros(modes)
    gold = 0.6180339887
    for m in range(modes):
        sq = (sq * 1664525 + 1013904223) % 4294967296
        th = 2 * np.pi * ((sq >> 8) / 16777216.0)
        snc[m] = np.cos(th); sns[m] = np.sin(th)
        fr = ((m + 1) * gold) % 1.0
        snw[m] = 2 * np.pi * sn_hz * (1.0 + sn_spread * (2.0 * fr - 1.0)) * 32.0 / sr
    snrc = np.cos(snw); snrs = np.sin(snw)
    amc = np.cos(am_ph); ams = np.sin(am_ph)
    lvib = 0.0
    use_hf = hf_g > 0.0
    fb0 = 1.0; fb1 = 0.0; fb2 = 0.0; fa1 = 0.0; fa2 = 0.0
    if use_hf:
        w0 = 2 * np.pi * min(hf_hz, sr * 0.45) / sr
        al = np.sin(w0) / (2 * 0.7071067811865476)
        cw = np.cos(w0); a0 = 1 + al
        fb0 = (1 + cw) / 2 / a0; fb1 = -(1 + cw) / a0; fb2 = fb0; fa1 = -2 * cw / a0; fa2 = (1 - al) / a0
    gx1 = 0.0; gx2 = 0.0; gy1 = 0.0; gy2 = 0.0; ux1 = 0.0; ux2 = 0.0; uy1 = 0.0; uy2 = 0.0
    qb0 = 1.0; qb1 = 0.0; qb2 = 0.0; qa1 = 0.0; qa2 = 0.0
    use_hlp = use_hf and hf_lp < sr * 0.45
    if use_hlp:
        w0 = 2 * np.pi * hf_lp / sr
        al = np.sin(w0) / (2 * 0.7071067811865476)
        cw = np.cos(w0); a0 = 1 + al
        qb0 = (1 - cw) / 2 / a0; qb1 = (1 - cw) / a0; qb2 = qb0; qa1 = -2 * cw / a0; qa2 = (1 - al) / a0
    vx1 = 0.0; vx2 = 0.0; vy1 = 0.0; vy2 = 0.0; wx1 = 0.0; wx2 = 0.0; wy1 = 0.0; wy2 = 0.0
    hfo = 0.0
    hq = s ^ 0x3C6EF372
    # (round 11, the cello's) The low partials ring on after the let-go: each partial's gain rises lr_w dB a second
    # (by partial: lr_dbs under lr_knee, less above) for lr_cap seconds once let go, against the fade.
    lr_now = 0.0
    # (round 11, the cello's) The gain envelope by partial and time knot (the table's kn_on and kn_rl by octave,
    # spread over the partials at note-on): kon [modes, knots] dB at the times kon_t from the note-on (0 at the last),
    # krl [modes, knots] dB at krl_t after the let-go (0 at the first, held after the last). Per block: one segment,
    # two multiply-adds a partial, in the exponent the block's gain glide already takes.
    use_kon = kon.shape[1] > 0
    use_ksu = ksu.shape[1] > 0
    use_krl = krl.shape[1] > 0
    kn_now = np.zeros(modes)
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
            lr_now = min(tr, lr_cap)
            if use_flr:
                xr = min(1.0, tr / max(1e-3, flr_rs))
                ftarget = (1.0 + (flr_rel - 1.0) * xr) * 10 ** (-flr_fade * tr / 20.0)
                fe_step = (ftarget - fe) / 32.0
        elif i % 32 == 0 and use_flr:
            fe_step = ((fe + fatt * (1.0 - fe)) - fe) / 32.0
        if i % 32 == 0 and rs_on:
            tb = (i + 32) / sr
            for l in range(nrs):
                if rs_p[l, 0] <= 0:
                    continue
                if i + 32 <= held:
                    if rs_p[l, 6] > 0:
                        gt = 0.0
                    else:
                        gt = 1.0 - np.exp(-tb / max(1e-4, rs_p[l, 1]))
                        if rs_p[l, 2] > 0:
                            gt *= np.exp(-tb / rs_p[l, 2])
                    rs_held[l] = gt
                else:
                    tr_ = (i + 32 - held) / sr
                    xr_ = min(1.0, tr_ / max(1e-4, rs_p[l, 4]))
                    if rs_p[l, 6] > 0:
                        gt = xr_ * np.exp(-DB * rs_p[l, 5] * tr_)
                    else:
                        gt = rs_held[l] * np.exp(-DB * (rs_p[l, 3] * xr_ + rs_p[l, 5] * tr_))
                        if rs_p[l, 2] > 0:
                            gt *= np.exp(-tr_ / rs_p[l, 2])
                # (round 12, the violin's) the table's gain envelope by layer and time knot (dB, amplitude):
                # rs_kon [layer, knots] at rs_kon_t from the note-on (0 at the last and after), rs_krl at rs_krl_t
                # after the let-go (0 at the let-go, the last held). Per block: one segment a layer.
                if use_rksu:
                    # (the layers' share of the held note's level over time: rs_ksu [layer, knots] at ksu_t)
                    if tb <= ksu_t[0]:
                        gt *= np.exp(DB * rs_ksu[l, 0])
                    elif tb >= ksu_t[-1]:
                        gt *= np.exp(DB * rs_ksu[l, -1])
                    else:
                        j = 0
                        while ksu_t[j + 1] <= tb:
                            j += 1
                        u = (tb - ksu_t[j]) / (ksu_t[j + 1] - ksu_t[j])
                        gt *= np.exp(DB * (rs_ksu[l, j] * (1.0 - u) + rs_ksu[l, j + 1] * u))
                if use_rkon and tb < rs_kon_t[-1]:
                    j = 0
                    while rs_kon_t[j + 1] <= tb:
                        j += 1
                    u = (tb - rs_kon_t[j]) / (rs_kon_t[j + 1] - rs_kon_t[j])
                    gt *= np.exp(DB * (rs_kon[l, j] * (1.0 - u) + rs_kon[l, j + 1] * u))
                if use_rkrl and i + 32 > held:
                    tk = (i + 32 - held) / sr
                    if tk >= rs_krl_t[-1]:
                        gt *= np.exp(DB * rs_krl[l, -1])
                    else:
                        j = 0
                        while rs_krl_t[j + 1] <= tk:
                            j += 1
                        u = (tk - rs_krl_t[j]) / (rs_krl_t[j + 1] - rs_krl_t[j])
                        gt *= np.exp(DB * (rs_krl[l, j] * (1.0 - u) + rs_krl[l, j + 1] * u))
                if use_rsm:
                    gt *= rsmask[l]
                rs_step[l] = (gt - rs_g[l]) / 32.0
        if i % 32 == 0:
            t = i / sr
            ob_now = obshape * np.exp(-t / ob_s)
            # (round 10) the hiss builds over hs_s from hs_depth down (the hair's top end comes in after the stroke starts)
            hs_env = 1.0 - hs_depth * np.exp(-t / hs_s)
            if use_kon or use_krl or use_ksu:
                kn_now = np.zeros(modes)
                if use_ksu:
                    # (round 12, the violin's) the held note's level over time (the table's kv_su by octave, dB at
                    # ksu_t from the note-on, held before the first knot and after the last, the release too)
                    if t <= ksu_t[0]:
                        kn_now = ksu[:, 0].copy()
                    elif t >= ksu_t[-1]:
                        kn_now = ksu[:, -1].copy()
                    else:
                        j = 0
                        while ksu_t[j + 1] <= t:
                            j += 1
                        u = (t - ksu_t[j]) / (ksu_t[j + 1] - ksu_t[j])
                        kn_now = ksu[:, j] * (1.0 - u) + ksu[:, j + 1] * u
                if use_kon and t < kon_t[-1]:
                    j = 0
                    while kon_t[j + 1] <= t:
                        j += 1
                    u = (t - kon_t[j]) / (kon_t[j + 1] - kon_t[j])
                    kn_now = kn_now + kon[:, j] * (1.0 - u) + kon[:, j + 1] * u
                if use_krl and i >= held:
                    tk = (i + 32 - held) / sr
                    if tk >= krl_t[-1]:
                        kn_now = kn_now + krl[:, -1]
                    else:
                        j = 0
                        while krl_t[j + 1] <= tk:
                            j += 1
                        u = (tk - krl_t[j]) / (krl_t[j + 1] - krl_t[j])
                        kn_now = kn_now + krl[:, j] * (1.0 - u) + krl[:, j + 1] * u
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
                if fl_hpa > 0.0:
                    flsl += fl_hpa * (flut - flsl)
                    flhp = flut - flsl
                else:
                    flhp = flut
                swing_db = fl_clip * np.tanh(env * flhp * fshape / fl_clip)
                dk = np.exp(-sigma * np.exp(-DB * swing_db) * k)
                fall = dk * dk
                turn = 2.0 * dk * np.cos(2 * np.pi * hz * k)
            else:
                turn = 2.0 * decay * np.cos(2 * np.pi * hz * k)
            swing = 2 * np.pi * rip_c * cents / 1200.0
            ripple = 0.5 * rip_db * rshape * (np.sin(ph1 + swing) - base1 + np.sin(ph2 + 1.618034 * swing) - base2)
            if use_sn:
                c_ = snc * snrc - sns * snrs
                sns = sns * snrc + snc * snrs
                snc = c_
                ripple = ripple + sn_amp * sns * env
            if use_am:
                lvib = (1.0 + vd) * min(1.0, max(0.0, (t - vib_delay - am_dly) / max(1e-3, vib_rise)))
                ripple = ripple + am_amp * lvib * (np.sin(phase) * amc + np.cos(phase) * ams)
            # (glided to over the block: a step would zip)
            out_step = (out0 * np.exp(DB * (ripple + wander + (0.0 if fl_damp > 0 else 1.0) * flut * fshape - ob_now - rdk * xdrop + lr_w * lr_now + kn_now)) - out) / 32.0
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
        if use_hf:
            if hf_src == 2:
                # (an own noise, as loud as the bow's speed: no correlation with the hair's noise)
                hq = (hq * 1664525 + 1013904223) % 4294967296
                hsrc = ((hq >> 8) * (2.0 / 16777216.0) - 1.0) * fac
            else:
                hsrc = force if hf_src == 0 else noise_out
            g1 = fb0 * hsrc + fb1 * gx1 + fb2 * gx2 - fa1 * gy1 - fa2 * gy2
            gx2 = gx1; gx1 = hsrc; gy2 = gy1; gy1 = g1
            hfo = fb0 * g1 + fb1 * ux1 + fb2 * ux2 - fa1 * uy1 - fa2 * uy2
            ux2 = ux1; ux1 = g1; uy2 = uy1; uy1 = hfo
            if use_hlp:
                h3 = qb0 * hfo + qb1 * vx1 + qb2 * vx2 - qa1 * vy1 - qa2 * vy2
                vx2 = vx1; vx1 = hfo; vy2 = vy1; vy1 = h3
                hfo = h3
                h4 = qb0 * hfo + qb1 * wx1 + qb2 * wx2 - qa1 * wy1 - qa2 * wy2
                wx2 = wx1; wx1 = hfo; wy2 = wy1; wy1 = h4
                hfo = h4
        qp = q; q = nxt
        out = out + out_step
        if i >= held:
            level *= fade
        rn = rosin * noise_out * 1e-4 * nboost * (hs_env if hs_rn > 0 else 1.0)
        if use_rhp:
            rf = rb0 * rn + rb1 * rx1 + rb2 * rx2 - ra1 * ry1 - ra2 * ry2
            rx2 = rx1; rx1 = rn; ry2 = ry1; ry1 = rf
            rn = rf
        if use_mm:
            bridge = (np.sum(out * mmask * q) + rn) * level
        else:
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
        # (round 10) the hair's noise into the body after the hum's high-pass: the body's air mode rings under a high note
        bin_ = bridge + rnb * rn * level
        for j in range(nb):
            # (round 10) b1_in: the hair's noise straight into the body's few modes between 300 and 500 Hz (0 for the rest)
            xin = bin_ + b1_in[j] * noise_out
            yy = bz_b0[j] * (xin - bx2[j]) - bz_a1[j] * by1[j] - bz_a2[j] * by2[j]
            bx2[j] = bx1[j]; bx1[j] = xin; by2[j] = by1[j]; by1[j] = yy
            snd += bz_g[j] * yy
        if use_hf:
            snd += hf_g * hfo * level
        if a0_g > 0.0:
            # (round 10) the air mode's own ring: the hair's noise (and a0_fl of the hall's floor) through one resonator
            # (a0_b0 (x - x2) - a1 y1 - a2 y2)
            ain = noise_out + a0_fl * fe * pnl
            ay = a0_b0 * (ain - ax2) - a0_a1 * ay1 - a0_a2 * ay2
            ax2 = ax1; ax1 = ain; ay2 = ay1; ay1 = ay
            snd += a0_g * ay
        # hiss: high-passed noise straight to the output (bow hair on the string)
        hp = hp_a * (hp_state + noise_out - hp_prev)
        hp_prev = noise_out; hp_state = hp
        raw = snd * outgain + hiss * hs_env * hp
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
            fo = fhp
            if flr_ord > 1:
                # (round 10) a second one-pole high-pass: Spitfire's floor is low under 200 Hz
                # (the violin's: flr_o2 = 1 - t of it, none on the cello's own floor)
                fhp2 = fha * (fhp2 + fhp - fhx2); fhx2 = fhp
                if flr_o2 >= 1.0:
                    fo = fhp2
                elif flr_o2 > 0.0:
                    fo = fhp + (fhp2 - fhp) * flr_o2
            fe += fe_step
            raw += flr_g * fe * (fo + flr_w * wn)
            pnl = pn
            if rb_g > 0.0:
                # (round 10) the hall's rumble: the floor's pink noise through a low resonator
                ry = rb_b0 * (pn - rbx2) - rb_a1 * rby1 - rb_a2 * rby2
                rbx2 = rbx1; rbx1 = pn; rby2 = rby1; rby1 = ry
                raw += rb_g * fe * ry
        lp = raw + lp_a * (lp - raw)
        y[i] = lp * ymask[0] if use_ym else lp
        if rs_on:
            in1 = rs_fin[rs_f1] if rs_f1 >= 0 else 0.0
            out1 = rs_fin[rs_fn - 1 - rs_f1] if rs_f1 >= 0 else 1.0
            in2 = rs_fin[rs_f2] if rs_f2 >= 0 else 0.0
            out2 = rs_fin[rs_fn - 1 - rs_f2] if rs_f2 >= 0 else 1.0
            for l in range(nrs):
                rs_g[l] += rs_step[l]
                y[i] += rs_g[l] * (rs_t1[l, rs_i1] * out1 + rs_t1[l, rs_j1] * in1 + rs_t2[l, rs_i2] * out2 + rs_t2[l, rs_j2] * in2)
            L1 = rs_t1.shape[1]; L2 = rs_t2.shape[1]
            rs_i1 += 1
            if rs_i1 >= L1:
                rs_i1 = 0
            if rs_f1 >= 0:
                rs_j1 += 1
                if rs_j1 >= L1:
                    rs_j1 = 0
                rs_f1 += 1
                if rs_f1 == rs_fn:
                    rs_i1 = rs_j1; rs_f1 = -1; rs_u1 = L1 - rs_fn
            else:
                rs_u1 -= 1
                if rs_u1 <= 0:
                    rs_rng = (rs_rng * 1664525 + 1013904223) % 4294967296
                    rs_j1 = (rs_rng >> 8) % L1; rs_f1 = 0
            rs_i2 += 1
            if rs_i2 >= L2:
                rs_i2 = 0
            if rs_f2 >= 0:
                rs_j2 += 1
                if rs_j2 >= L2:
                    rs_j2 = 0
                rs_f2 += 1
                if rs_f2 == rs_fn:
                    rs_i2 = rs_j2; rs_f2 = -1; rs_u2 = L2 - rs_fn
            else:
                rs_u2 -= 1
                if rs_u2 <= 0:
                    rs_rng = (rs_rng * 1664525 + 1013904223) % 4294967296
                    rs_j2 = (rs_rng >> 8) % L2; rs_f2 = 0
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
               rosin=0.017985, jitter=0.08536, rel_s=0.30149, fade_db=22.334, hiss=0.014072, hiss_hp=8571.2, top=12000.0,
               attack_scale=0.050102, lp_hz=5701.8,
               rip_db=3.326, rip_c=19.46, slow_db=2.052, slow_hz=3.0, loud_db=1.742,
               vib_rate_dev=0.09024, vib_depth_dev=0.4561, vib_drift_hz=0.8, direct_db=-12.074, ring=9.3783, lift=1.0, ring_s=0.4991,
               fl_db=21.35, fl_hz=16.51, fl_slope=0.0, fl_hzslope=0.7853, jit_ct=0.0, jit_hz=20.0, pdrift_ct=1.441, pdrift_hz=0.5,
               fl_knee=818.2, fl_fslope=1.285, rip_knee=963.9, rip_fslope=2.074, direct_hp=57.747, fl_damp=1.0, fl_clip=30.0,
               direct_lp=20000.0, rosin_hp=0.0, jit_hp=0.0,
               ob_db=17.49, ob_knee=2689.6, ob_slope=2.9034, ob_s=0.15823,
               ob_ksz=1.56, sw_sz=0.25,
               room_db=-2.5, room_sz=-4.1611, room_ms=18.44, room_lo=38.88, room_hi=159.5, rloss=1.2938, rpow=3.2245,
               room_lk=1.5661, room_hk=0.13181, rloss_k=10.877,
               ring_k=-3.0806, rpow_k=1.1345, rg_k=1.9217, fade_k=0.39364,
               dir_sz=-7.6977, dhp_k=0.87, vib_k=-0.098769, obs_k=0.96,
               rdrop_db=3.092, rdrop_s=0.02, tail_db=22.875, tail_s=0.50944, tail_t0=0.07799, sw2=0.67352, nz_amt=0.30009, ntail=0.34346,
               hum_k=0.51353, hum_all=1.0,
               flr_db=-55.793, flr_k=4.9665, flr_lp=1090.5, flr_hp=186.75, flr_att=0.0035533, flr_rel_db=4.3191, flr_rs=0.05,
               flr_fade=23.941, flr_w=0.30072, hiss_k=3.8134,
               nfloor_db=-5.0, nfloor_dbs=47.443, rdk_s=0.03,
               rnb=0.0, rnb_k=0.0, a0_db=-90.79, a0_k=-0.11, a0_hz=277.3, a0_bw=9.59, flr_ord=2, a0_fl=3.4784, rb_db=0.55926, rb_hz=69.667, rb_bw=52.777, hiss_cap=990.38, hiss_db=0.0, b1_db=-96.808, b1_k=2.5579, b1_lo=300.0, b1_hi=500.0, hs_depth=0.7625, hs_s=0.30139, lp_k=0.0, hs_rn=1,
               # (round 10, the cello's: all weighted by t, nothing at a violin's size)
               hf_db=-140.0, hf_k=0.0, hf_f=1.0, hf_src=2, hf_lp=6500.0,
               sn_db=3.7854, sn_hz=0.74098, sn_spread=0.36811, sn_knee=392.17, sn_slope=0.19059, sn_lo=1.1794,
               fl_hpz=1.2142, am_db=1.8913, am_knee=428.76, am_slope=2.0, am_ph=1.1911, am_dly=0.0,
               cflr_db=-49.364, cflr_k=2.8205, cflr_lp=205.97, cflr_hp=30.0, cflr_att=0.02, cflr_rel_db=-8.7946,
               cflr_rs=0.05, cflr_fade=23.1, cflr_w=0.0,
               # (round 11, the cello's: by t, nothing at a violin's size)
               lr_dbs=9.7764, lr_knee=260.42, lr_slope=3.2865, lr_cap=0.59455,
               kn_on_t=(0.0, 0.05, 0.1, 0.2, 0.35, 0.6, 0.9), kn_rl_t=(0.0, 0.05, 0.15, 0.3, 0.45, 0.6),
               # (round 12, the violin's) the layers' knot times (the table's rk_on, rk_rl)
               rk_on_t=(0.0, 0.05, 0.1, 0.2, 0.35, 0.6, 0.9), rk_rl_t=(0.0, 0.05, 0.15, 0.3, 0.45, 0.6),
               su_t=(1.0, 1.9, 2.8, 3.7),
               # (round 11, the violin's) the residual layers: 0 sustain, 1 onset, 2 release
               rs_replace=1,
               rs0_on=1, rs0_att=0.25, rs0_tau=0.0, rs0_rdb=10.0, rs0_rs=0.05, rs0_fade=60.0,
               rs1_on=1, rs1_att=0.005, rs1_tau=0.25, rs1_rdb=0.0, rs1_rs=0.05, rs1_fade=0.0,
               rs2_on=1, rs2_att=0.0, rs2_tau=0.0, rs2_rdb=0.0, rs2_rs=0.15, rs2_fade=40.0,
               # (round 12, the cello's) what the cello's layers replace (0..1, by their weight; by t already)
               rsc_cflr=1.0, rsc_hf=1.0, rsc_nfl=1.0, rsc_hiss=0.0)

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
             attack_scale=0.05, bright=2.144, loss1=3.947e-06, direct_db=-10.33, direct_hp=96.61, lp_hz=3000.0,   # (round 12: lp_hz 5590 -> 3000, under the layers)
             rosin=0.01695, hum_k=0.33, hiss_hp=8000.0, hiss_k=0.0,
             hiss=0.0022,
             slow_hz=0.6, hs_depth=0.0, lp_k=0.0,   # (round 10: the violin's retuned wander corner and hair onset blend back)
             # (round 10, the cello's, blended in by t like the rest)
             slow_db=0.093356, loud_db=0.77633, rip_db=2.1124, fl_db=13.929, vib_depth_dev=0.31309, vib_rate=5.0418, vib_rise=0.46505, vib_delay=0.12956, fl_hz=22.958)

# (Round 9) The per-note tables, each fitted at one size (notes_<inst>.json; write_engine.py puts
# them in ModalBowedNotes.h). Per note: gain_db (dB on each partial's share of the bridge force),
# dark_db (dB more onset darkening), loss_add (1/s more released loss), drop_db (each partial's own
# drop at the let-go), and scalars that replace the value at that size for that note (before the
# size powers). Read at note-on by the nearest MIDI note, never between notes (the violin fit found
# that worse than no table). A table is all there at its own size and gone a quarter of the way
# (in log size) toward the other: weight 1 - 4 |u - u_table|, u = log(size) / log(2.1).
_HERE = os.path.dirname(os.path.abspath(__file__))
NOTES = [json.load(open(os.path.join(_HERE, "notes_%s.json" % inst))) for inst in ("violin", "cello")]
TABLE_SCALARS = ("flr_db", "tail_db", "rdrop_db", "fade_db", "ob_db", "swell_s", "vib_ct", "slow_db", "rip_db", "rel_s", "ring",
                 "hiss_db", "a0_db", "sn_db", "am_db")   # (round 10: hiss_db, dB on the hiss; a0_db, the air mode's level)
TABLE_ARRAYS = ("gain_db", "dark_db", "loss_add", "drop_db")
# (round 12, the cello's: sn_db, am_db, the sine wander's and the level vibrato's depth per note)
# (round 11) and the cello's kn_on [8 octaves][6 knots] and kn_rl [8][5] (dB): the gain envelope by octave and time knot


def note_rows(size, note, tables=None, layer=False):
    """[(weight, row)] for a note at a body size: the tables that reach this size and note. (round 12)
    layer=True: [(weight, row, env)], the residual layers' weight instead, w (1 - |t - u_table|): w (1 - t)
    for the violin's table (as round 11) and w t for the cello's; env the table's own layer envelope
    (rs_env: rs<l>_att, _tau, _rdb, _rs, _fade in place of DEFAULT's; the violin's table has none)."""
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
        if layer:
            w *= max(0.0, 1.0 - abs(min(1.0, max(0.0, u)) - np.log(tab["size"]) / np.log(2.1)))
            if w > 0.0:
                rows.append((w, tab["notes"][str(n)], tab.get("rs_env", {})))
            continue
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
    p.update(kw.get("_post", {}))   # (fitting only: values that override the tables' scalars too)
    f0 = 440 * 2 ** ((note - 69) / 12)
    # (round 10) per-note additive dB columns of the cello's table: hf_add (the string's top), cflr_add (the low floor)
    hf_add = sum(w * row.get("hf_add", 0.0) for w, row in rows)
    cflr_add = sum(w * row.get("cflr_add", 0.0) for w, row in rows)
    hf_add += kw.get("hf_addk", 0.0); cflr_add += kw.get("cflr_addk", 0.0)   # (fitting only: a table column's value by hand)
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
    # (round 10, the cello's) The low floor: the violin's hall-floor noise, with the cello's own
    # level, band and fade blended in by t (the violin's floor fades out by 1 - t as before).
    flr_gv = (1.0 - t) * 10 ** (p["flr_db"] / 20) * (f0 / 440.0) ** (p["flr_k"] / 6.0206) if t < 1.0 and p["flr_db"] > -150 else 0.0
    # (round 11, the violin's) The residual layers replace the hall floor, its rumble, the air mode and the
    # 300-500 Hz modes' noise wherever a table with them reaches (keep = 1 - its weight; 1 on a cello).
    keep = 1.0 - min(1.0, sum(w for w, row in rows if "rs0_db" in row)) if p["rs_replace"] > 0 else 1.0
    flr_gv *= keep
    flr_gc = t * 10 ** ((p["cflr_db"] + cflr_add) / 20) * (f0 / 440.0) ** (p["cflr_k"] / 6.0206) if t > 0.0 and p["cflr_db"] > -150 else 0.0
    # (round 12, the cello's) The residual layers on a cello (the cello table's rs columns, by w t) replace
    # rsc_cflr of the low floor, rsc_hf of the string's top and rsc_nfl of the release floor (each 0..1, by
    # the layers' weight; all three are by t already, so nothing changes at a violin's size).
    lrows = note_rows(SIZES[inst], note, tables, layer=True)
    wl = min(1.0, sum(w for w, row, env in lrows if "rs0_db" in row))
    flr_gc *= 1.0 - p["rsc_cflr"] * wl
    fp = {}
    for key in ("lp", "hp", "att", "rel_db", "rs", "fade", "w"):
        fp[key] = p["flr_" + key] if flr_gc == 0.0 else (p["flr_" + key] + (p["cflr_" + key] - p["flr_" + key]) * t)
    hf_hz = p["hf_f"] * modes * f0 * np.sqrt(1.0 + p["b"] * modes * modes)
    hf_g = t * 10 ** ((p["hf_db"] + hf_add) / 20) if p["hf_db"] + hf_add > -150 else 0.0
    hf_g = hf_g * (f0 / 65.406) ** (p["hf_k"] / 6.0206) if t > 0.0 and p["hf_db"] > -150 else 0.0
    hf_g *= 1.0 - p["rsc_hf"] * wl
    # (round 10, the cello's, by t) the sine wander's and the level vibrato's depth by partial
    hzm = np.minimum(np.arange(1, modes + 1) * f0, SR * 0.49)
    sn_amp = t * p["sn_db"] * np.where(hzm < p["sn_knee"], p["sn_lo"], (hzm / p["sn_knee"]) ** -p["sn_slope"]) if p["sn_db"] != 0.0 else np.zeros(modes)
    am_amp = t * p["am_db"] / (1.0 + (hzm / p["am_knee"]) ** p["am_slope"]) if p["am_db"] != 0.0 else np.zeros(modes)
    flrg = _flrg(p, f0, t)
    # (round 11, the cello's, by t) the low partials' ring-on after the let-go: lr_dbs dB a second, as
    # 1 / (1 + (hz / lr_knee) ^ lr_slope) by partial
    lr_w = t * p["lr_dbs"] / (1.0 + (hzm / p["lr_knee"]) ** p["lr_slope"]) if t > 0.0 and p["lr_dbs"] != 0.0 else np.zeros(modes)
    rg, rl, rfb, rp = room_coeffs(dict(p, room_db=p["room_db"] + p["room_sz"] * np.log2(sz),
                                       room_lo=p["room_lo"] * sz ** p["room_lk"], room_hi=p["room_hi"] * sz ** p["room_hk"]))
    return kernel(f0, int(seconds * SR), int(held * SR), SR, modes, p["beta"], p["loss0"], loss1, p["b"], p["grip"],
                  0.05 + 0.3 * p["speed"], p["vel"], seed, p["vib_ct"], p["vib_rate"], p["vib_delay"], p["vib_rise"],
                  attack, p["swell_s"], p["swell_amt"], p["rosin"], p["jitter"], p["rel_s"], p["fade_db"],
                  b0, a1, a2, g, 30000.0, (1.0 - p["rsc_hiss"] * wl * t) * p["hiss"] * 10 ** (p["hiss_db"] / 20) * (min(f0, p["hiss_cap"]) / 440.0) ** p["hiss_k"], p["hiss_hp"], p["lp_hz"] * (f0 / 440.0) ** p["lp_k"],
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
                  flr_gv + flr_gc,
                  fp["lp"], fp["hp"], fp["att"], 10 ** (fp["rel_db"] / 20), fp["rs"], fp["fade"], fp["w"],
                  _arr(rows, "gain_db") + np.resize(np.asarray(kw.get("gain_addk", 0.0), float), 64), _arr(rows, "dark_db"), _arr(rows, "loss_add"), _arr(rows, "drop_db"), p["rdk_s"],
                  (1.0 - p["rsc_nfl"] * wl) * t * 10 ** (p["nfloor_db"] / 20) if p["nfloor_db"] > -100 else 0.0, p["nfloor_dbs"],
                  (1.0 - t) * p["rnb"] * (f0 / 440.0) ** p["rnb_k"],
                  keep * (1.0 - t) * 10 ** (p["a0_db"] / 20) * (f0 / 440.0) ** (p["a0_k"] / 6.0206) if p["a0_db"] > -150 else 0.0, *_reso(p["a0_hz"], p["a0_bw"]), int(p["flr_ord"]),
                  _a0fl(p, f0, t), keep * flrg * 10 ** (p["rb_db"] / 20) if p["rb_db"] > -150 else 0.0, *_reso(p["rb_hz"], p["rb_bw"]),
                  keep * _b1(p, f0, t, inst), p["hs_depth"], max(1e-3, p["hs_s"]), p["hs_rn"],
                  hf_g, hf_hz, int(p["hf_src"]), p["hf_lp"],
                  sn_amp, p["sn_hz"], p["sn_spread"], am_amp, p["am_ph"], p["am_dly"],
                  t * (1.0 - np.exp(-2 * np.pi * p["fl_hpz"] * 32.0 / SR)) if p["fl_hpz"] > 0 else 0.0, 1.0 - t,
                  lr_w, max(0.0, p["lr_cap"]), *_knots(p, rows, kw, hzm, t),
                  *_layers(p, rows, lrows, note, t, kw, hzm),
                  np.asarray(kw.get("_mmask", np.zeros(0)), float), np.asarray(kw.get("_ymask", np.zeros(0)), float),
                  np.asarray(kw.get("_rsmask", np.zeros(0)), float))


# (round 11, the violin's) The residual layers. Each layer's spectrum is the note's table column
# rs<l>_db (45 values: dB of the band energy in look.py's 1/6-octave bands as the score's STFT reads it,
# for an envelope of 1), faded out toward the cello by (1 - t) in amplitude. The envelope's globals
# rs<l>_att, _tau, _rdb, _rs, _fade (DEFAULT) or the row's own. Loops: L1 = 16384 and L2 = 14641 (11 ^ 4)
# samples (coprime), random phase (fixed per note and layer), each carrying half the energy; each jumps to
# a random place once a length, crossfading over 15 ms (kernel: rs_fn), so it doesn't repeat. Nothing is
# stored but the table: the loops are synthesised from its 45 band levels (the engine once per process).
# (65536/59049 scored 10.38, 16384/14641 10.40, 8192/6561 10.43 on seeds 4-6; the longer pair has more
# bins in the lowest bands and repeats less audibly.)
RS_L1, RS_L2 = 16384, 14641
RS_LAYERS = 3
_RSCAL = {}
_RSTAB = {}


def _rs_bins(L):
    import look
    f = np.fft.rfftfreq(L, 1.0 / SR)
    fs = np.fft.rfftfreq(2048, 1.0 / SR)
    E = look.EDGES
    eds = [(a, b) for a, b in zip(E, E[1:]) if ((fs >= a) & (fs < b)).any()]
    idx = np.full(len(f), -1)
    for j, (a, b) in enumerate(eds):
        idx[(f >= a) & (f < b)] = j
    return idx


def _rs_noise(amp_band, L, key):
    idx = _rs_bins(L)
    rng = np.random.default_rng(key)
    ph = np.exp(2j * np.pi * rng.random(len(idx)))
    a = np.where(idx >= 0, amp_band[np.maximum(idx, 0)], 0.0)
    return np.fft.irfft(a * ph, L)


def _rs_cal():
    # band energy (look.bandgram's units) per unit band amplitude, each loop alone
    if not _RSCAL:
        import look
        for L in (RS_L1, RS_L2):
            x = _rs_noise(np.ones(45), L, 12345)
            x = np.tile(x, int(np.ceil(4.8 * SR / L)) + 1)[:int(4.8 * SR)]
            t, B = look.bandgram(x)
            _RSCAL[L] = np.mean(10 ** (B[:, (t > 0.3) & (t < 4.5)] / 10), 1)
    return _RSCAL


def rs_tables(db, key):
    k = (tuple(np.round(db, 6)), key)
    if k not in _RSTAB:
        cal = _rs_cal()
        E = 10 ** (np.asarray(db, float) / 10) * 0.5
        v = tuple(_rs_noise(np.sqrt(E / cal[L]), L, key * 7 + L) for L in (RS_L1, RS_L2))
        if len(_RSTAB) > 400:
            _RSTAB.clear()
        _RSTAB[k] = v
    return _RSTAB[k]


def _res_layers(p, rows, note, t, kw):
    """rows: note_rows(..., layer=True), each table's weight already times (1 - |u - u_table|)."""
    t1 = np.zeros((RS_LAYERS, 1)); t2 = np.zeros((RS_LAYERS, 1)); P = np.zeros((RS_LAYERS, 7))
    cols = []
    env = {}
    for l in range(RS_LAYERS):
        key = "rs%d_db" % l
        db = kw.get(key + "k")
        if db is None:
            for w, row, e in rows:
                if key in row and w > 0:
                    db = np.asarray(row[key], float) + 20 * np.log10(w)
                    env = e
        cols.append(db)
    if all(c is None for c in cols):
        return t1, t2, P
    t1 = np.zeros((RS_LAYERS, RS_L1)); t2 = np.zeros((RS_LAYERS, RS_L2))
    for l, db in enumerate(cols):
        if db is None or p["rs%d_on" % l] <= 0:
            continue
        a, b = rs_tables(db, int(round(note * 10)) * 3 + l)
        t1[l] = a; t2[l] = b
        q = dict(p); q.update({k: v for k, v in env.items() if k not in kw})   # (round 12: the table's own envelope; kw holds)
        P[l] = (1.0, q["rs%d_att" % l], q["rs%d_tau" % l], q["rs%d_rdb" % l], q["rs%d_rs" % l], q["rs%d_fade" % l], 1.0 if l == 2 else 0.0)
    return t1, t2, P


def _knots(p, rows, kw, hzm, t):
    """(round 11, the cello's) the knots' gain envelope by partial: the table's kn_on [8 octaves][knots] and kn_rl, by t,
    interpolated over the partials in log frequency (octave centres 110 Hz to 14 kHz, held past the ends); kn_on's last
    knot (kn_on_t[-1]) and kn_rl's first (the let-go) are 0. kw kn_onk / kn_rlk (fitting) add."""
    res = []
    for key, tk, first in (("kn_on", p["kn_on_t"], False), ("kn_rl", p["kn_rl_t"], True)):
        tk = np.asarray(tk, float)
        V = np.zeros((8, len(tk) - 1))
        hit = False
        for w, row in rows:
            if key in row:
                V += w * np.asarray(row[key], float); hit = True
        if key + "k" in kw:
            V += np.asarray(kw[key + "k"], float); hit = True
        # (round 12, the violin's) kv_on / kv_rl: the same envelope in the violin's table, by the table's weight (not t)
        kv = "kv" + key[2:]
        Vv = np.zeros((8, len(tk) - 1))
        hitv = False
        for w, row in rows:
            if kv in row:
                Vv += w * np.asarray(row[kv], float); hitv = True
        if kv + "k" in kw:
            Vv += np.asarray(kw[kv + "k"], float); hitv = True
        hitv = hitv and np.any(Vv != 0.0)
        hitc = t > 0.0 and hit and np.any(V != 0.0)
        if not hitc and not hitv:
            res += [np.zeros((len(hzm), 0)), tk]
            continue
        cen = np.log2(110.0) + np.arange(8)
        W = np.array([np.interp(np.log2(hzm), cen, np.eye(8)[k]) for k in range(8)]).T   # [modes, 8]
        pad = lambda A: np.concatenate([np.zeros((8, 1)), A], 1) if first else np.concatenate([A, np.zeros((8, 1))], 1)
        M = t * W @ pad(V) if hitc else np.zeros((len(hzm), len(tk)))
        if hitv:
            M = M + W @ pad(Vv)
        res += [np.ascontiguousarray(M), tk]
    return res


def _layers(p, rows, lrows, note, t, kw, hzm):
    """The residual layers with their knots: (rs_t1, rs_t2, rs_p, rs_kon, rs_kon_t, rs_krl, rs_krl_t, ksu, su_t, rs_ksu).
    rows: note_rows(...) (the knots, by the table's weight); lrows: note_rows(..., layer=True) (the layers, round 12's cello)."""
    return (*_res_layers(p, lrows, note, t, kw), *_rs_knots(p, rows, kw, t), *_su_knots(p, rows, kw, hzm, t))


def _su_knots(p, rows, kw, hzm, t):
    """(round 12, the violin's) the held note's level over time: the violin table's kv_su [8 octaves][len(su_t)] (dB on each
    partial's gain at su_t from the note-on, held before the first knot and after the last; spread over the partials as
    kv_on) and rk_su [3 layers][len(su_t)] (dB on each layer), by the table's weight; kw kv_suk / rk_suk (fitting) add.
    Returns (ksu [modes, knots], su_t, rs_ksu [3, knots])."""
    tk = np.asarray(p["su_t"], float)
    out = []
    for key, nrow in (("kv_su", 8), ("rk_su", RS_LAYERS)):
        V = np.zeros((nrow, len(tk)))
        hit = False
        for w, row in rows:
            if key in row:
                V += w * np.asarray(row[key], float); hit = True
        if key + "k" in kw:
            V += np.asarray(kw[key + "k"], float); hit = True
        out.append(V if (hit and t < 1.0 and np.any(V != 0.0)) else None)
    if out[0] is None:
        ksu = np.zeros((len(hzm), 0))
    else:
        cen = np.log2(110.0) + np.arange(8)
        W = np.array([np.interp(np.log2(hzm), cen, np.eye(8)[k]) for k in range(8)]).T
        ksu = np.ascontiguousarray(W @ out[0])
    rsu = np.zeros((RS_LAYERS, 0)) if out[1] is None else np.ascontiguousarray(out[1])
    return ksu, tk, rsu


def _rs_knots(p, rows, kw, t):
    """(round 12, the violin's) the layers' gain envelope: the violin table's rk_on [3 layers][len(rk_on_t) - 1] (dB at
    rk_on_t from the note-on; 0 at the last knot and after) and rk_rl [3][len(rk_rl_t) - 1] (dB at rk_rl_t after the let-go;
    0 at the let-go, the last held), by the table's weight; kw rk_onk / rk_rlk (fitting) add."""
    res = []
    for key, first in (("rk_on", False), ("rk_rl", True)):
        tk = np.asarray(p[key + "_t"], float)
        V = np.zeros((RS_LAYERS, len(tk) - 1))
        hit = False
        for w, row in rows:
            if key in row:
                V += w * np.asarray(row[key], float); hit = True
        if key + "k" in kw:
            V += np.asarray(kw[key + "k"], float); hit = True
        if t >= 1.0 or not hit or not np.any(V != 0.0):
            res += [np.zeros((RS_LAYERS, 0)), tk]
            continue
        Z = np.zeros((RS_LAYERS, 1))
        res += [np.ascontiguousarray(np.concatenate([Z, V], 1) if first else np.concatenate([V, Z], 1)), tk]
    return res


def _b1(p, f0, t, inst):
    hz = np.array([h for h in BODY["hz"] if h / SIZES[inst] < SR * 0.45]) / SIZES[inst]
    g = (1.0 - t) * 10 ** (p["b1_db"] / 20) * (f0 / 440.0) ** (p["b1_k"] / 6.0206) if p["b1_db"] > -150 else 0.0
    return np.where((hz >= p["b1_lo"]) & (hz <= p["b1_hi"]), g, 0.0)


def _a0g(p, f0, t):
    return (1.0 - t) * 10 ** (p["a0_db"] / 20) * (f0 / 440.0) ** (p["a0_k"] / 6.0206) if p["a0_db"] > -150 else 0.0


def _flrg(p, f0, t):
    return (1.0 - t) * 10 ** (p["flr_db"] / 20) * (f0 / 440.0) ** (p["flr_k"] / 6.0206) if t < 1.0 and p["flr_db"] > -150 else 0.0


def _a0fl(p, f0, t):
    # (the floor's share into the air mode, a0_fl against the floor's own level; the air mode's output gain is a0_g)
    g = _a0g(p, f0, t)
    return p["a0_fl"] * _flrg(p, f0, t) / g / 30000.0 if g > 0 else 0.0


def _reso(hz, bw, sr=SR):
    r = np.exp(-np.pi * bw / sr)
    return (1 - r * r) * 0.5, -2 * r * np.cos(2 * np.pi * hz / sr), r * r
