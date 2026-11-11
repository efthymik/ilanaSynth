"""Write fitted constants (a fit_look.py json) into src/dsp/ModalBowed.h, and the per-note
tables (notes_violin.json, notes_cello.json) into src/dsp/ModalBowedNotes.h."""
import json
import os
import re
import sys

import sim

ENGINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "dsp", "ModalBowed.h")
p = dict(sim.DEFAULT)
p.update({k: v for k, v in json.load(open(sys.argv[1])).items() if k in sim.DEFAULT})
s = open(ENGINE).read()


def number(v, digits=5):
    text = ("%.1f" % v) if abs(v) >= 1000 else ("%%.%dg" % digits % v) if abs(v) >= 1e-3 or v == 0 else ("%%.%de" % (digits - 1) % v)
    return text if ("." in text or "e" in text) else text + ".0"


def put(name, value, suffix="f", digits=5):
    global s
    pattern = r"(\b%s = )[-0-9.e+]+%s" % (name, suffix)
    assert re.search(pattern, s), name
    s = re.sub(pattern, lambda m: m.group(1) + number(value, digits) + suffix, s)


for name, key in (("rosinJitter", "jitter"), ("hissLevel", "hiss"), ("brightness", "bright"), ("topHz", "top"), ("tiltHz", "lp_hz"),
                  ("directDb", "direct_db"), ("rippleDb", "rip_db"), ("rippleDensity", "rip_c"), ("wanderDb", "slow_db"), ("bowDriftDb", "loud_db")):
    put(name, p[key])
put("bodyNoise", p["rosin"] * 1e-4)
put("releaseSeconds", p["rel_s"], "")
put("releaseFadeDb", p["fade_db"], "")
put("releaseRing", p["ring"], "")
put("releaseRingSeconds", p["ring_s"], "")
put("flutterDb", p["fl_db"])
put("flutterSlope", p["fl_slope"])
put("flutterHzSlope", p["fl_hzslope"])
put("pitchDriftCents", p["pdrift_ct"])
put("flutterHz", p["fl_hz"], "")
put("rateWander", p["vib_rate_dev"])
put("depthWander", p["vib_depth_dev"])
put("flutterKnee", p["fl_knee"])
put("flutterFreqSlope", p["fl_fslope"])
put("rippleKnee", p["rip_knee"])
put("rippleFreqSlope", p["rip_fslope"])
put("directHp", p["direct_hp"])
put("flutterClip", p["fl_clip"])
# (round 6: the onset, the box's darker top, the room, the release's losses)
put("swellDepth", p["swell_amt"], "")
put("swellSeconds", p["swell_s"], "")
put("onsetDarkDb", p["ob_db"], "")
put("onsetDarkKnee", p["ob_knee"], "")
put("onsetDarkSlope", p["ob_slope"], "")
put("onsetDarkSeconds", p["ob_s"], "")
put("directLp", p["direct_lp"])
put("releaseLossHz", p["rloss"], "")
put("releaseLossSlope", p["rpow"])
# (the engine has room_sz in two parts, roomSizeDb + roomSizeDbMore: the second stays)
put("roomSizeDb", p["room_sz"] - float(re.search(r"roomSizeDbMore = ([-0-9.e+]+)", s).group(1)), "", digits=6)
for name, key in (("roomDb", "room_db"), ("roomMs", "room_ms"), ("roomLowDecay", "room_lo"), ("roomHighDecay", "room_hi")):
    put(name, p[key], "")
# (round 7: the cello's size powers; roomSizeDb and roomSizeDbMore together are room_sz)
put("onsetDarkKneeSize", p["ob_ksz"])
put("swellSize", p["sw_sz"], "")
put("releaseLossSizeSlope", p["rloss_k"], digits=5)
put("roomLowSizeSlope", p["room_lk"], "")
put("roomHighSizeSlope", p["room_hk"], "")
# (round 8: the hum's high-pass, the second swell stage, the noise's swell, the let-go's two
# stages; the cello's size powers; sim.CELLO, the values the violin's terms blend to at the
# cello's size: the engine's ...Cello constants)
for name, key in (("humRatio", "hum_k"),):
    put(name, p[key])
for name, key in (("swellSecondStage", "sw2"), ("noiseSwellDepth", "nz_amt"), ("letGoDropDb", "rdrop_db"), ("letGoDropSeconds", "rdrop_s"),
                  ("tailDb", "tail_db"), ("tailSeconds", "tail_s"), ("tailDelay", "tail_t0"), ("tailNoise", "ntail"),
                  ("releaseRingSizeSlope", "ring_k"), ("releaseLossSlopeSizeSlope", "rpow_k"), ("releaseRampSizeSlope", "rg_k"),
                  ("releaseFadeSizeSlope", "fade_k"), ("directSizeDb", "dir_sz"), ("directHpSizeSlope", "dhp_k"),
                  ("vibratoSizeSlope", "vib_k"), ("onsetDarkSecondsSizeSlope", "obs_k")):
    put(name, p[key], "")
for name, key in (("swellSecondsCello", "swell_s"), ("swellDepthCello", "swell_amt"), ("onsetDarkDbCello", "ob_db"), ("onsetDarkKneeCello", "ob_knee"),
                  ("onsetDarkSlopeCello", "ob_slope"), ("onsetDarkSecondsCello", "ob_s"), ("releaseSecondsCello", "rel_s"),
                  ("releaseFadeDbCello", "fade_db"), ("releaseRingCello", "ring"), ("releaseLossHzCello", "rloss")):
    put(name, sim.CELLO[key], "")
# (the engine has the cello's second stage, let-go stages and tail noise off, and its noise on the stroke's own swell)
assert sim.CELLO["sw2"] == 0 and sim.CELLO["rdrop_db"] == 0 and sim.CELLO["tail_db"] == 0 and sim.CELLO["ntail"] == 0 and sim.CELLO["nz_amt"] == sim.CELLO["swell_amt"]
assert p["hum_all"] == 1 and p["lift"] == 1
put("stringLossHigh", p["loss1"])
# (round 9: the violin's retuned globals and hall floor, the cello's flutter, wander, ripple, hiss
# and release floor; sim.CELLO's values for the globals the violin retuned are the engine's ...Cello)
for name, key in (("attackScale", "attack_scale"), ("hissHp", "hiss_hp"), ("hissSlope", "hiss_k"), ("hallFloorDb", "flr_db"),
                  ("hallFloorSlope", "flr_k"), ("hallFloorLowHz", "flr_lp"), ("hallFloorHighHz", "flr_hp"), ("hallFloorRise", "flr_att"),
                  ("hallFloorLetGoDb", "flr_rel_db"), ("hallFloorLetGoSeconds", "flr_rs"), ("hallFloorFade", "flr_fade"),
                  ("hallFloorWhite", "flr_w"), ("noiseFloorDb", "nfloor_db"), ("noiseFloorFade", "nfloor_dbs"),
                  ("partialDropSeconds", "rdk_s")):
    put(name, p[key], "", digits=5)
for name, key, scale in (("brightnessCello", "bright", 1.0), ("stringLossHighCello", "loss1", 1.0), ("directDbCello", "direct_db", 1.0),
                         ("directHpCello", "direct_hp", 1.0), ("tiltHzCello", "lp_hz", 1.0), ("bodyNoiseCello", "rosin", 1e-4),
                         ("humRatioCello", "hum_k", 1.0), ("attackScaleCello", "attack_scale", 1.0), ("hissHpCello", "hiss_hp", 1.0),
                         ("hissLevelCello", "hiss", 1.0), ("flutterDbCello", "fl_db", 1.0), ("wanderDbCello", "slow_db", 1.0),
                         ("bowDriftDbCello", "loud_db", 1.0), ("rippleDbCello", "rip_db", 1.0)):
    put(name, sim.CELLO[key] * scale, "", digits=5)
assert sim.CELLO["hiss_k"] == 0
put("vibratoFitted", p["vib_ct"], "")
# (round 10, the violin's: the air mode, the hair's noise into the 300-500 Hz modes, the floor's rumble,
# the hiss's cap and onset, the wander's corner; rnb and lp_k are the fit's only, not in the engine)
for name, key in (("airDb", "a0_db"), ("airSlope", "a0_k"), ("airHz", "a0_hz"), ("airBandwidth", "a0_bw"), ("airFloor", "a0_fl"),
                  ("hairBodyDb", "b1_db"), ("hairBodySlope", "b1_k"), ("hairBodyLowHz", "b1_lo"), ("hairBodyHighHz", "b1_hi"),
                  ("rumbleDb", "rb_db"), ("rumbleHz", "rb_hz"), ("rumbleBandwidth", "rb_bw"), ("hissCap", "hiss_cap"),
                  ("hairOnsetDepth", "hs_depth"), ("hairOnsetSeconds", "hs_s"), ("wanderHz", "slow_hz")):
    put(name, p[key], "")
put("wanderHzCello", sim.CELLO["slow_hz"], "")
assert p["rnb"] == 0 and p["lp_k"] == 0 and p["flr_ord"] == 2 and p["hs_rn"] == 1 and p["hiss_db"] == 0
assert sim.CELLO["hs_depth"] == 0 and sim.CELLO["lp_k"] == 0
# (round 10, the cello's: the vibrato's and flutter's values at the cello's size, the sine wander, the
# flutter's high-pass, the level vibrato, the low floor and the string's top, all by t)
for name, key in (("vibratoRateCello", "vib_rate"), ("vibratoDelayCello", "vib_delay"), ("vibratoRiseCello", "vib_rise"),
                  ("flutterHzCello", "fl_hz"), ("depthWanderCello", "vib_depth_dev")):
    put(name, sim.CELLO[key], "")
for name, key in (("sineWanderDb", "sn_db"), ("sineWanderHz", "sn_hz"), ("sineWanderSpread", "sn_spread"), ("sineWanderKnee", "sn_knee"),
                  ("sineWanderSlope", "sn_slope"), ("sineWanderLow", "sn_lo"), ("flutterHighPassHz", "fl_hpz"),
                  ("levelVibratoDb", "am_db"), ("levelVibratoKnee", "am_knee"), ("levelVibratoSlope", "am_slope"), ("levelVibratoLead", "am_ph"),
                  ("levelVibratoDelay", "am_dly"), ("lowFloorDb", "cflr_db"), ("lowFloorSlope", "cflr_k"), ("lowFloorLowHz", "cflr_lp"),
                  ("lowFloorHighHz", "cflr_hp"), ("lowFloorRise", "cflr_att"), ("lowFloorLetGoDb", "cflr_rel_db"),
                  ("lowFloorLetGoSeconds", "cflr_rs"), ("lowFloorFade", "cflr_fade"), ("lowFloorWhite", "cflr_w"),
                  ("stringTopDb", "hf_db"), ("stringTopSlope", "hf_k"), ("stringTopAt", "hf_f"), ("stringTopLowPassHz", "hf_lp")):
    put(name, p[key], "")
# (the engine's string top is its own noise (hf_src 2), low-passed (hf_lp under 0.45 of the rate))
assert p["hf_src"] == 2 and p["hf_lp"] < 48000 * 0.45
# (round 11, the cello's: the low partials' ring-on once let go, by t; the knots' times are in ModalBowedNotes.h)
for name, key in (("releaseRiseDb", "lr_dbs"), ("releaseRiseKnee", "lr_knee"), ("releaseRiseSlope", "lr_slope"), ("releaseRiseSeconds", "lr_cap")):
    put(name, p[key], "")
# (round 12, the cello's: what its residual layers replace, by their level (by t already); the hiss stays)
for name, key in (("layersReplaceLowFloor", "rsc_cflr"), ("layersReplaceStringTop", "rsc_hf"), ("layersReplaceReleaseFloor", "rsc_nfl")):
    put(name, p[key], "")
assert p["rsc_hiss"] == 0
open(ENGINE, "w").write(s)

# (round 9) The per-note tables (notes_<inst>.json) as ModalBowedNotes.h.
NOTES_H = os.path.join(os.path.dirname(ENGINE), "ModalBowedNotes.h")
SCALARS = (("floorDb", "flr_db"), ("tailDb", "tail_db"), ("letGoDropDb", "rdrop_db"), ("fadeDb", "fade_db"), ("onsetDarkDb", "ob_db"),
           ("swellSeconds", "swell_s"), ("vibratoCents", "vib_ct"), ("wanderDb", "slow_db"), ("rippleDb", "rip_db"),
           ("releaseSeconds", "rel_s"), ("releaseRing", "ring"), ("hissDb", "hiss_db"), ("airDb", "a0_db"),
           ("sineWanderDb", "sn_db"), ("levelVibratoDb", "am_db"))
assert tuple(k for _, k in SCALARS) == sim.TABLE_SCALARS
# (round 10, the cello's: columns added to a value by the table's weight, not replacing it)
SCALARS += (("stringTopAddDb", "hf_add"), ("lowFloorAddDb", "cflr_add"))
ARRAYS = (("gainDb", "gain_db"), ("darkDb", "dark_db"), ("lossAdd", "loss_add"), ("dropDb", "drop_db"))


def floats(v, digits):
    v = list(v)
    while v and v[-1] == 0:
        v.pop()
    return "{ " + ", ".join(number(x, digits) + "f" for x in v) + " }" if v else "{}"


# (Round 11, the violin's) the residual layers' bands (look.py's with STFT bins, as sim._rs_bins) and each
# loop's calibration (sim._rs_cal: band energy per unit bin amplitude, as the score's STFT reads it).
import look  # noqa: E402
import numpy as np  # noqa: E402
_fs = np.fft.rfftfreq(2048, 1.0 / sim.SR)
RS_EDGES = [(a, b) for a, b in zip(look.EDGES, look.EDGES[1:]) if ((_fs >= a) & (_fs < b)).any()]
RS_CAL = sim._rs_cal()


# (Round 10) The tables to 6 digits, so the header holds the json (round 9's 5 rounded a few).
DIGITS = {"violin": 6, "cello": 6}


out = ["#pragma once", "",
       "// The Modal Bowed exciter's per-note tables (ModalBowed.h, round 9), each fitted at one body size",
       "// to ilana's Spitfire String Quartet renders through the engine's Python twin. Per MIDI note:",
       "// gainDb (dB on each partial's share of the bridge force, index 0 the fundamental), darkDb (dB",
       "// more onset darkening), lossAdd (1/s more loss once let go), dropDb (each partial's own drop at",
       "// the let-go), and scalars that replace the engine's value at that size for that note (scalars:",
       "// a bit for each one the table sets). A table is read at note-on by the nearest MIDI note (never",
       "// between notes), all there at its own size and gone a quarter of the way (in log size) toward",
       "// the next; below first it is held (holdBelow) or absent, above last it fades over fadeAbove",
       "// semitones (0: absent). Another instrument's table is one more Row array and Table entry.",
       "// Generated (tools/modal_bowed_refit/write_engine.py from notes_<inst>.json): edit the tools, not this file.",
       "struct ModalBowedNotes",
       "{",
       "    static constexpr int partials = 64;",
       "    // (round 11, the cello's) The gain envelope by octave (centres 110 Hz x 2 ^ k) and time knot: knotOn dB at",
       "    // knotOnSeconds[0..5] from the note-on (0 at the last and after), knotRelease at knotReleaseSeconds[1..5]",
       "    // after the let-go (0 at the let-go, the last held), each partial's by its log frequency between the centres.",
       "    static constexpr int knotOctaves = 8, knotOnCount = %d, knotReleaseCount = %d;" % (len(sim.DEFAULT["kn_on_t"]) - 1, len(sim.DEFAULT["kn_rl_t"]) - 1),
       "    static constexpr double knotOnSeconds[knotOnCount + 1] { %s };" % ", ".join(number(x) for x in sim.DEFAULT["kn_on_t"]),
       "    static constexpr double knotReleaseSeconds[knotReleaseCount + 1] { %s };" % ", ".join(number(x) for x in sim.DEFAULT["kn_rl_t"]),
       "    // (round 12, the violin's) The same envelope in the violin's table (its kv_on, kv_rl in knotOn, knotRelease), and",
       "    // the held note's level over time: knotSustain dB by octave at knotSustainSeconds from the note-on, held before the",
       "    // first and after the last (the release too). And each residual layer's envelope on the same knots: layerOn dB",
       "    // (amplitude) at knotOnSeconds[0..5] (0 at the last and after), layerRelease at knotReleaseSeconds[1..5] after the",
       "    // let-go (0 at the let-go, the last held). All by the table's weight.",
       "    static constexpr int knotSustainCount = %d;" % len(sim.DEFAULT["su_t"]),
       "    static constexpr double knotSustainSeconds[knotSustainCount] { %s };" % ", ".join(number(x) for x in sim.DEFAULT["su_t"]),
       "    // (round 11, the violin's) The residual layers (ModalBowedLayers.h): each note's noise between and under the",
       "    // partials as three layers (0 the sustain's, 1 the onset's, 2 the release's), each two noise loops of",
       "    // residualLengths samples synthesised once per process from residualDb: dB of the band energy in each of",
       "    // residualBands bands (residualEdges, Hz) as the picture score's STFT (2048 Hann, 48 kHz) reads it, at an",
       "    // envelope of 1 (-200: none). A loop bin in band b gets amplitude sqrt (10 ^ (dB / 10) / 2 / residualScale",
       "    // [loop][b]) with a random phase. The layers' envelopes: held, (1 - exp (-t / attack)) (x exp (-t / tau)",
       "    // when tau > 0); once let go, the held value letGoDb down over letGoSeconds and fadeDb a second (x exp",
       "    // (-t / tau)); a release layer is 0 while held and rises over letGoSeconds once let go, fading fadeDb a second.",
       "    static constexpr int residualLayers = 3, residualBands = %d;" % len(RS_EDGES),
       "    static constexpr int residualLengths[2] { %d, %d };" % (sim.RS_L1, sim.RS_L2),
       "    static constexpr double residualEdges[residualBands][2] { %s };" % ", ".join("{ %r, %r }" % (float(a), float(b)) for a, b in RS_EDGES),
       "    static constexpr double residualScale[2][residualBands] { %s };" % ", ".join("{ %s }" % ", ".join("%r" % float(x) for x in RS_CAL[L]) for L in (sim.RS_L1, sim.RS_L2)),
       "    // (round 12) Each table's own layer envelope (Table::envelope: the twin's rs_env over its DEFAULT).",
       "    struct Envelope",
       "    {",
       "        double attack[residualLayers], tau[residualLayers], letGoDb[residualLayers], letGoSeconds[residualLayers], fadeDb[residualLayers];",
       "    };",
       "    static constexpr bool residualRelease[residualLayers] { false, false, true };",
       "    // The layers replace the hall floor, its rumble, the air mode and the hair's noise into the 300-500 Hz",
       "    // modes by the residual table's weight (residualReplace); (round 12) and, by their level, the cello's",
       "    // low floor, string top and release floor (ModalBowed.h, layersReplace...).",
       "    static constexpr bool residualReplace = %s;" % ("true" if p["rs_replace"] > 0 else "false"),
       "    enum Scalar { " + ", ".join(n for n, _ in SCALARS) + ", scalarCount };",
       "    struct Row",
       "    {",
       "        float " + " {}, ".join("%s[partials]" % n for n, _ in ARRAYS) + " {};",
       "        float scalar[scalarCount] {};",
       "        float knotOn[knotOctaves][knotOnCount] {}, knotRelease[knotOctaves][knotReleaseCount] {};",
       "        float residualDb[residualLayers][residualBands] {};",
       "        float knotSustain[knotOctaves][knotSustainCount] {};",
       "        float layerOn[residualLayers][knotOnCount] {}, layerRelease[residualLayers][knotReleaseCount] {};",
       "    };",
       "    struct Table",
       "    {",
       "        double size;          // the body size it was fitted at (a violin's 1)",
       "        int first, last;      // MIDI notes",
       "        bool holdBelow;",
       "        int fadeAbove;        // semitones",
       "        unsigned scalars;     // 1 << Scalar for each one the table sets",
       "        const Row* rows;      // first..last",
       "        bool knots;           // (round 11) any row has knotOn or knotRelease",
       "        bool sustainKnots;    // (round 12) any row has knotSustain",
       "        bool layerKnots;      // (round 12) any row has layerOn or layerRelease",
       "        bool residual;        // (round 11) every row has residualDb",
       "        Envelope envelope;    // (round 12) its residual layers' envelope",
       "    };"]
tables = []
for inst in ("violin", "cello"):
    tab = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "notes_%s.json" % inst)))
    assert sorted(map(int, tab["notes"])) == list(range(tab["first"], tab["last"] + 1))
    mask = 0
    rows = []
    for n in range(tab["first"], tab["last"] + 1):
        row = tab["notes"][str(n)]
        for i, (_, key) in enumerate(SCALARS):
            if key in row:
                mask |= 1 << i
        rows.append(row)
    for row in rows:
        assert all((key in row) == bool(mask >> i & 1) for i, (_, key) in enumerate(SCALARS))
        assert all(len(row.get(key, [])) <= 64 for _, key in ARRAYS)
    out.append("    static constexpr Row %sRows[%d] {" % (inst, len(rows)))
    for n, row in zip(range(tab["first"], tab["last"] + 1), rows):
        out.append("        { %s, // %d" % (", ".join(floats(row.get(key, []), DIGITS[inst]) for _, key in ARRAYS), n))
        knots = ", {}, {}"   # (every member given: a default initializer can't be read before the struct is complete)
        # (round 12: the violin's kv_on, kv_rl in the same columns; a table has one or the other)
        kn = "kn" if "kn_on" in tab["notes"][str(tab["first"])] else "kv" if "kv_on" in tab["notes"][str(tab["first"])] else None
        if kn is not None:
            assert not ("kn_on" in row and "kv_on" in row)
            assert all(len(row[kn + key]) == 8 and all(len(v) == len(sim.DEFAULT["kn" + key + "_t"]) - 1 for v in row[kn + key]) for key in ("_on", "_rl"))
            knots = ",\n          { %s },\n          { %s }" % tuple(", ".join(floats(v, DIGITS[inst]) for v in row[kn + key]) for key in ("_on", "_rl"))
        residual = ", {}"
        if "rs0_db" in row:
            assert all(len(row["rs%d_db" % l]) == len(RS_EDGES) for l in range(3))
            residual = ",\n          { %s }" % ", ".join("{ %s }" % ", ".join(number(x, DIGITS[inst]) + "f" for x in row["rs%d_db" % l]) for l in range(3))
        # (round 12, the violin's) kv_su, rk_on, rk_rl (the layers' knots share kn_on_t, kn_rl_t)
        assert tuple(sim.DEFAULT["rk_on_t"]) == tuple(sim.DEFAULT["kn_on_t"]) and tuple(sim.DEFAULT["rk_rl_t"]) == tuple(sim.DEFAULT["kn_rl_t"])
        more = ", {}, {}, {}"
        if any(key in row for key in ("kv_su", "rk_on", "rk_rl")):
            assert len(row["kv_su"]) == 8 and all(len(v) == len(sim.DEFAULT["su_t"]) for v in row["kv_su"])
            assert all(len(row[key]) == 3 and all(len(v) == len(sim.DEFAULT[key + "_t"]) - 1 for v in row[key]) for key in ("rk_on", "rk_rl"))
            more = ",\n          { %s },\n          { %s },\n          { %s }" % tuple(", ".join(floats(v, DIGITS[inst]) for v in row[key]) for key in ("kv_su", "rk_on", "rk_rl"))
        assert "rk_su" not in row   # (the twin's hook, unused: not in the engine)
        out.append("          %s%s%s%s }," % (floats([row.get(key, 0.0) for _, key in SCALARS] if mask else [], DIGITS[inst]), knots, residual, more))
    out.append("    };")
    assert all(("rs0_db" in row) == ("rs0_db" in rows[0]) for row in rows)
    # (round 12) the table's layer envelope: its rs_env over DEFAULT's (sim._res_layers)
    env = dict(p)
    env.update(tab.get("rs_env", {}))
    envelope = "{ %s }" % ", ".join("{ %s }" % ", ".join(number(env["rs%d_%s" % (l, k)]) for l in range(3)) for k in ("att", "tau", "rdb", "rs", "fade"))
    flag = lambda *keys: "true" if any(key in row for row in rows for key in keys) else "false"
    tables.append("{ %s, %d, %d, %s, %d, 0x%xu, %sRows, %s, %s, %s, %s, %s }" % (number(tab["size"]), tab["first"], tab["last"],
                                                                                 "true" if tab["hold_below"] else "false", tab["fade_above"], mask, inst,
                                                                                 flag("kn_on", "kn_rl", "kv_on", "kv_rl"), flag("kv_su"), flag("rk_on", "rk_rl"),
                                                                                 "true" if "rs0_db" in rows[0] else "false", envelope))
out += ["    static constexpr int tableCount = %d;" % len(tables),
        "    static constexpr Table tables[tableCount] { %s };" % (", ".join(tables)),
        "};", ""]
open(NOTES_H, "w").write("\n".join(out))
print({k: p[k] for k in ("loss1", "jitter", "hiss", "rosin", "bright", "top", "lp_hz", "rip_db", "rip_c", "slow_db", "loud_db", "direct_db", "rel_s", "fade_db", "ring", "ring_s", "fl_db", "fl_hz", "fl_slope", "fl_hzslope", "pdrift_ct", "vib_rate_dev", "vib_depth_dev", "fl_knee", "fl_fslope", "rip_knee", "rip_fslope", "direct_hp", "fl_clip", "swell_amt", "swell_s", "ob_db", "ob_knee", "ob_slope", "ob_s", "direct_lp", "rloss", "rpow", "room_db", "room_sz", "room_ms", "room_lo", "room_hi")})
